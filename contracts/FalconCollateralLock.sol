// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/// @title FalconCollateralLock
/// @notice Locks EVM USDC until Falcon Ledger mints matching QUC (bridge-in)
///         and releases USDC after QUC burn/return (bridge-out).
/// @dev Mainnet-ready custody model: N-of-M owner multi-sig for all privileged
///      operations (withdraw / release / ownership changes). Testnet can set
///      required = 1 with a single owner.
interface IERC20 {
    function transferFrom(address from, address to, uint256 amount) external returns (bool);
    function transfer(address to, uint256 amount) external returns (bool);
}

contract FalconCollateralLock {
    IERC20 public immutable usdc;

    // ── Multi-sig owners ──────────────────────────────────────────────────
    mapping(address => bool) public isOwner;
    address[] public owners;
    uint256 public required; // confirmations needed (1..owners.length)

    struct DepositRecord {
        address sender;
        uint256 amount;
        string falconAccount;
        bool released;
    }

    mapping(bytes32 => DepositRecord) public deposits;
    uint256 public nextDepositNonce;

    mapping(bytes32 => bool) public processedWithdrawals;

    // opHash => owner => confirmed
    mapping(bytes32 => mapping(address => bool)) public confirmations;
    mapping(bytes32 => uint256) public confirmationCount;
    mapping(bytes32 => bool) public executed;

    event DepositCreated(
        bytes32 indexed depositId,
        address indexed sender,
        uint256 amount,
        string falconAccount
    );
    event DepositReleased(bytes32 indexed depositId, address indexed recipient, uint256 amount);
    event WithdrawalReleased(
        bytes32 indexed withdrawalId,
        address indexed recipient,
        uint256 amount,
        string falconAccount,
        string falconTxHash
    );
    event OwnerAdded(address indexed owner);
    event OwnerRemoved(address indexed owner);
    event RequirementChanged(uint256 required);
    event OperationConfirmed(bytes32 indexed opHash, address indexed owner, uint256 count);
    event OperationExecuted(bytes32 indexed opHash);

    modifier onlyOwner() {
        require(isOwner[msg.sender], "not owner");
        _;
    }

    /// @param usdcToken USDC token address (must not be zero)
    /// @param initialOwners Multi-sig owner set (unique, non-zero)
    /// @param requiredConfirmations Threshold in [1, initialOwners.length]
    constructor(address usdcToken, address[] memory initialOwners, uint256 requiredConfirmations) {
        require(usdcToken != address(0), "zero usdc");
        require(initialOwners.length > 0, "no owners");
        require(
            requiredConfirmations > 0 && requiredConfirmations <= initialOwners.length,
            "bad required"
        );

        usdc = IERC20(usdcToken);
        required = requiredConfirmations;

        for (uint256 i = 0; i < initialOwners.length; ++i) {
            address o = initialOwners[i];
            require(o != address(0), "zero owner");
            require(!isOwner[o], "duplicate owner");
            isOwner[o] = true;
            owners.push(o);
            emit OwnerAdded(o);
        }
        emit RequirementChanged(requiredConfirmations);
    }

    function ownerCount() external view returns (uint256) {
        return owners.length;
    }

    /// @notice Lock USDC and tag with the recipient's Falcon Ledger address (r...).
    function deposit(uint256 amount, string calldata falconAccount) external returns (bytes32 depositId) {
        require(amount > 0, "amount required");
        require(bytes(falconAccount).length > 0, "falcon account required");
        require(usdc.transferFrom(msg.sender, address(this), amount), "transferFrom failed");

        depositId = keccak256(
            abi.encodePacked(
                block.chainid,
                address(this),
                nextDepositNonce++,
                msg.sender,
                amount,
                falconAccount
            )
        );

        deposits[depositId] = DepositRecord({
            sender: msg.sender,
            amount: amount,
            falconAccount: falconAccount,
            released: false
        });

        emit DepositCreated(depositId, msg.sender, amount, falconAccount);
    }

    // ── Multi-sig helpers ─────────────────────────────────────────────────

    function _confirm(bytes32 opHash) internal returns (uint256 count) {
        require(isOwner[msg.sender], "not owner");
        require(!executed[opHash], "already executed");
        require(!confirmations[opHash][msg.sender], "already confirmed");
        confirmations[opHash][msg.sender] = true;
        count = ++confirmationCount[opHash];
        emit OperationConfirmed(opHash, msg.sender, count);
    }

    function _markExecuted(bytes32 opHash) internal {
        require(confirmationCount[opHash] >= required, "insufficient confirmations");
        require(!executed[opHash], "already executed");
        executed[opHash] = true;
        emit OperationExecuted(opHash);
    }

    /// @notice Hash for a bridge-out USDC release (after Falcon QUC return).
    function withdrawOpHash(
        uint256 amount,
        address recipient,
        bytes32 withdrawalId,
        string calldata falconAccount,
        string calldata falconTxHash
    ) public pure returns (bytes32) {
        return keccak256(
            abi.encode(
                "withdraw",
                amount,
                recipient,
                withdrawalId,
                falconAccount,
                falconTxHash
            )
        );
    }

    /// @notice Confirm a withdraw op. Executes automatically at threshold.
    /// public (not external) so required==1 withdraw() alias can call it in-contract.
    function confirmWithdraw(
        uint256 amount,
        address recipient,
        bytes32 withdrawalId,
        string calldata falconAccount,
        string calldata falconTxHash
    ) public onlyOwner {
        require(amount > 0, "amount required");
        require(recipient != address(0), "zero recipient");
        require(!processedWithdrawals[withdrawalId], "already processed");
        require(bytes(falconAccount).length > 0, "falcon account required");
        require(bytes(falconTxHash).length > 0, "falcon tx required");

        bytes32 opHash = withdrawOpHash(amount, recipient, withdrawalId, falconAccount, falconTxHash);
        _confirm(opHash);

        if (confirmationCount[opHash] >= required) {
            _markExecuted(opHash);
            processedWithdrawals[withdrawalId] = true;
            require(usdc.transfer(recipient, amount), "transfer failed");
            emit WithdrawalReleased(withdrawalId, recipient, amount, falconAccount, falconTxHash);
        }
    }

    /// @notice Hash for releasing a deposit back to a recipient (refund path).
    function releaseOpHash(bytes32 depositId, address recipient) public pure returns (bytes32) {
        return keccak256(abi.encode("release", depositId, recipient));
    }

    /// @notice Confirm a deposit release. Executes automatically at threshold.
    /// public so required==1 release() alias can call it in-contract.
    function confirmRelease(bytes32 depositId, address recipient) public onlyOwner {
        DepositRecord storage record = deposits[depositId];
        require(record.amount > 0, "unknown deposit");
        require(!record.released, "already released");
        require(recipient != address(0), "zero recipient");

        bytes32 opHash = releaseOpHash(depositId, recipient);
        _confirm(opHash);

        if (confirmationCount[opHash] >= required) {
            _markExecuted(opHash);
            record.released = true;
            require(usdc.transfer(recipient, record.amount), "transfer failed");
            emit DepositReleased(depositId, recipient, record.amount);
        }
    }

    // ── Owner set management (also multi-sig) ─────────────────────────────

    function addOwnerOpHash(address newOwner) public pure returns (bytes32) {
        return keccak256(abi.encode("addOwner", newOwner));
    }

    function confirmAddOwner(address newOwner) external onlyOwner {
        require(newOwner != address(0), "zero owner");
        require(!isOwner[newOwner], "already owner");

        bytes32 opHash = addOwnerOpHash(newOwner);
        _confirm(opHash);

        if (confirmationCount[opHash] >= required) {
            _markExecuted(opHash);
            isOwner[newOwner] = true;
            owners.push(newOwner);
            emit OwnerAdded(newOwner);
        }
    }

    function removeOwnerOpHash(address ownerToRemove) public pure returns (bytes32) {
        return keccak256(abi.encode("removeOwner", ownerToRemove));
    }

    function confirmRemoveOwner(address ownerToRemove) external onlyOwner {
        require(isOwner[ownerToRemove], "not an owner");
        require(owners.length > required, "would break threshold");

        bytes32 opHash = removeOwnerOpHash(ownerToRemove);
        _confirm(opHash);

        if (confirmationCount[opHash] >= required) {
            _markExecuted(opHash);
            isOwner[ownerToRemove] = false;
            for (uint256 i = 0; i < owners.length; ++i) {
                if (owners[i] == ownerToRemove) {
                    owners[i] = owners[owners.length - 1];
                    owners.pop();
                    break;
                }
            }
            emit OwnerRemoved(ownerToRemove);
        }
    }

    function changeRequirementOpHash(uint256 newRequired) public pure returns (bytes32) {
        return keccak256(abi.encode("changeRequirement", newRequired));
    }

    function confirmChangeRequirement(uint256 newRequired) external onlyOwner {
        require(newRequired > 0 && newRequired <= owners.length, "bad required");

        bytes32 opHash = changeRequirementOpHash(newRequired);
        _confirm(opHash);

        if (confirmationCount[opHash] >= required) {
            _markExecuted(opHash);
            required = newRequired;
            emit RequirementChanged(newRequired);
        }
    }

    // ── Legacy single-call aliases (only when required == 1) ──────────────
    // Kept so 1-of-1 testnet deploys keep a simple API. Mainnet must use
    // required >= 2 and confirm* functions only.

    function withdraw(
        uint256 amount,
        address recipient,
        bytes32 withdrawalId,
        string calldata falconAccount,
        string calldata falconTxHash
    ) external onlyOwner {
        require(required == 1, "use confirmWithdraw (multi-sig)");
        confirmWithdraw(amount, recipient, withdrawalId, falconAccount, falconTxHash);
    }

    function release(bytes32 depositId, address recipient) external onlyOwner {
        require(required == 1, "use confirmRelease (multi-sig)");
        confirmRelease(depositId, recipient);
    }
}

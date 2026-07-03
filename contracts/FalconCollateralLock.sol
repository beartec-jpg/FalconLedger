// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/// @title FalconCollateralLock
/// @notice Locks Sepolia USDC until Falcon validators attest and mint (or testnet owner releases).
/// @dev Testnet v1: deployer is owner. Production: transfer ownership to validator multisig.
interface IERC20 {
    function transferFrom(address from, address to, uint256 amount) external returns (bool);
    function transfer(address to, uint256 amount) external returns (bool);
}

contract FalconCollateralLock {
    IERC20 public immutable usdc;
    address public owner;

    struct DepositRecord {
        address sender;
        uint256 amount;
        string falconAccount;
        bool released;
    }

    mapping(bytes32 => DepositRecord) public deposits;
    uint256 public nextDepositNonce;

    event DepositCreated(
        bytes32 indexed depositId,
        address indexed sender,
        uint256 amount,
        string falconAccount
    );
    event DepositReleased(bytes32 indexed depositId, address indexed recipient, uint256 amount);
    event OwnershipTransferred(address indexed previousOwner, address indexed newOwner);

    modifier onlyOwner() {
        require(msg.sender == owner, "not owner");
        _;
    }

    constructor(address usdcToken) {
        require(usdcToken != address(0), "zero usdc");
        usdc = IERC20(usdcToken);
        owner = msg.sender;
    }

    /// @notice Lock USDC and tag with the recipient's Falcon Ledger address (r...).
    /// @param amount USDC amount in token units (6 decimals on Sepolia Circle USDC).
    /// @param falconAccount Falcon address string, e.g. rN7n7otQDd6FczFgLdlqtyMVQ...
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

    /// @notice Testnet / interim release back to depositor. Replace with validator-signed release on mainnet.
    function release(bytes32 depositId, address recipient) external onlyOwner {
        DepositRecord storage record = deposits[depositId];
        require(record.amount > 0, "unknown deposit");
        require(!record.released, "already released");

        record.released = true;
        require(usdc.transfer(recipient, record.amount), "transfer failed");
        emit DepositReleased(depositId, recipient, record.amount);
    }

    function transferOwnership(address newOwner) external onlyOwner {
        require(newOwner != address(0), "zero owner");
        emit OwnershipTransferred(owner, newOwner);
        owner = newOwner;
    }
}
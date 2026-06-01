#!/bin/bash
#
# setup-deploy-user.sh
# Creates a restricted 'qxrp-deploy' user with limited sudo rights
# for secure automated deployment by Grok.
#
# Usage:
#   sudo bash setup-deploy-user.sh "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIPwQtGOG9jcUAQIwaa5UkGq6byggFZvhBmSQEfEsTaIm grok-qxrp-deploy-20260601"
#

set -euo pipefail

DEPLOY_USER="qxrp-deploy"
PUBLIC_KEY="$1"

if [[ -z "$PUBLIC_KEY" ]]; then
    echo "Usage: sudo $0 'ssh-ed25519 AAAA... comment'"
    exit 1
fi

echo "==> Creating restricted deploy user: $DEPLOY_USER"

# Create user if it doesn't exist
if ! id "$DEPLOY_USER" &>/dev/null; then
    useradd -m -s /bin/bash "$DEPLOY_USER"
    usermod -aG sudo "$DEPLOY_USER"
    echo "$DEPLOY_USER ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/99-qxrp-deploy
    chmod 440 /etc/sudoers.d/99-qxrp-deploy
    echo "User $DEPLOY_USER created with full sudo (we will restrict it next)"
fi

# Set up SSH
DEPLOY_HOME="/home/$DEPLOY_USER"
mkdir -p "$DEPLOY_HOME/.ssh"
chmod 700 "$DEPLOY_HOME/.ssh"

# Add the provided public key (overwrite for safety)
echo "$PUBLIC_KEY" > "$DEPLOY_HOME/.ssh/authorized_keys"
chmod 600 "$DEPLOY_HOME/.ssh/authorized_keys"
chown -R "$DEPLOY_USER:$DEPLOY_USER" "$DEPLOY_HOME/.ssh"

echo "==> SSH key installed for $DEPLOY_USER"

# Create a much more restricted sudoers file
cat > /etc/sudoers.d/qxrp-deploy << 'SUDOEOF'
# Restricted sudo for qxrp-deploy user
# Only allow commands needed for qXRP node deployment and management

Defaults    env_reset
Defaults    secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

# Allow specific safe commands without password
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/systemctl restart qxrp*, /bin/systemctl stop qxrp*, /bin/systemctl start qxrp*, /bin/systemctl status qxrp*, /bin/systemctl daemon-reload
qxrp-deploy ALL=(ALL) NOPASSWD: /usr/bin/docker, /usr/bin/docker-compose
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/journalctl -u qxrp*
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/mkdir -p /var/lib/qxrp*
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/chown -R qxrp-deploy:qxrp-deploy /var/lib/qxrp*
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/rm -rf /var/lib/qxrp*
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/cp /tmp/*.service /etc/systemd/system/
qxrp-deploy ALL=(ALL) NOPASSWD: /bin/systemctl enable qxrp*

# Explicitly deny dangerous commands
qxrp-deploy ALL=(ALL) !/usr/bin/passwd
qxrp-deploy ALL=(ALL) !/usr/sbin/useradd
qxrp-deploy ALL=(ALL) !/usr/sbin/userdel
qxrp-deploy ALL=(ALL) !/usr/sbin/usermod
qxrp-deploy ALL=(ALL) !/bin/su
qxrp-deploy ALL=(ALL) !/usr/bin/sudo su
SUDOEOF

chmod 440 /etc/sudoers.d/qxrp-deploy

# Remove the overly permissive one we created earlier (if it exists)
rm -f /etc/sudoers.d/99-qxrp-deploy

echo ""
echo "✅ Deploy user '$DEPLOY_USER' has been set up with restricted sudo rights."
echo ""
echo "You can now SSH as this user:"
echo "  ssh qxrp-deploy@YOUR_SERVER_IP"
echo ""
echo "To remove this user later, run:"
echo "  sudo userdel -r qxrp-deploy && sudo rm -f /etc/sudoers.d/qxrp-deploy"
echo ""
echo "IMPORTANT: This user currently has broad permissions for deployment."
echo "After the new testnet is stable, you should either:"
echo "  1. Delete this user, or"
echo "  2. Further restrict its sudo rights."
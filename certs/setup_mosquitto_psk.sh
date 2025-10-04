#!/bin/bash
# Instalação e configuração do Mosquitto com TLS-PSK e ACL
# Para Debian/Ubuntu

set -e

echo "[1/6] Instalando Mosquitto e dependências..."
sudo apt update
sudo apt install -y mosquitto mosquitto-clients

echo "[2/6] Criando diretórios de configuração..."
sudo mkdir -p /etc/mosquitto/config
sudo mkdir -p /var/lib/mosquitto
sudo mkdir -p /var/log/mosquitto

echo "[3/6] Criando arquivo de configuração principal..."
cat <<EOF | sudo tee /etc/mosquitto/mosquitto.conf > /dev/null
per_listener_settings true

# Listener sem TLS (para debug/testes)
listener 1883
allow_anonymous true

# Listener com TLS-PSK
listener 8883
psk_hint hint00
psk_file /etc/mosquitto/config/psk.txt
use_identity_as_username true
acl_file /etc/mosquitto/config/acl.conf

pid_file /run/mosquitto/mosquitto.pid
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
EOF

echo "[4/6] Criando arquivo de chaves PSK..."
cat <<EOF | sudo tee /etc/mosquitto/config/psk.txt > /dev/null
aluno01:ABCD01EF1234
aluno02:ABCD02EF1234
aluno03:ABCD03EF1234
aluno79:ABCD79EF1234
EOF
# Adicione mais alunos conforme necessário

echo "[5/6] Criando arquivo ACL..."
cat <<EOF | sudo tee /etc/mosquitto/config/acl.conf > /dev/null
user aluno01
topic readwrite /aluno01/#

user aluno02
topic readwrite /aluno02/#

user aluno79
topic readwrite /aluno79/#
EOF
# Adicione mais regras conforme os alunos

echo "[6/6] Ajustando permissões e reiniciando Mosquitto..."
sudo chown mosquitto: /etc/mosquitto/config/psk.txt
sudo chmod 600 /etc/mosquitto/config/psk.txt
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto

echo "✅ Configuração concluída!"
echo "Teste com:"
echo "  mosquitto_sub -h 127.0.0.1 -p 8883 --psk-identity aluno79 --psk ABCD79EF1234 -t /aluno79/teste -d"
echo "  mosquitto_pub -h 127.0.0.1 -p 8883 --psk-identity aluno79 --psk ABCD79EF1234 -t /aluno79/teste -m 'hello via PSK' -d"

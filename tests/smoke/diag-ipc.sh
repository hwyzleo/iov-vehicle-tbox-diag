#!/bin/bash
# Smoke test for tbox-diag service IPC/config.
# Exit 0 = pass, non-zero = fail.
#
# 不下发真实 VIN、Seed/Key 或证书；仅验证配置落位、socket 可用性、
# DoIP 监听端口与服务生命周期。

set -euo pipefail

# --- 配置文件存在性（diag-runtime 安装 diag.default.yaml -> conf.d/diag.yaml）---
CONFIG_FILE="/etc/tbox/conf.d/diag.yaml"
if [ ! -f "${CONFIG_FILE}" ]; then
    echo "FAIL: config file ${CONFIG_FILE} not found"
    exit 1
fi

# --- conf.d 不得内联顶层 common:（common 由 BUILD 唯一提供）---
if grep -qE '^common:' "${CONFIG_FILE}"; then
    echo "FAIL: ${CONFIG_FILE} must not declare a top-level 'common:' block"
    exit 1
fi

# --- common.yaml 存在性（framework-config 的强制层）---
if [ ! -f /etc/tbox/common.yaml ]; then
    echo "FAIL: /etc/tbox/common.yaml not found (framework-config requires it)"
    exit 1
fi

# --- 自身 IPC socket ---
SOCKET_PATH="/tmp/tbox-diag.sock"
if [ ! -S "${SOCKET_PATH}" ]; then
    echo "FAIL: IPC socket ${SOCKET_PATH} not found"
    exit 1
fi

# --- 下游 socket（DIAG 启动时连接 PROV/SEC，失败即 exit 1）---
for dep in /tmp/tbox-prov.sock /tmp/tbox-sec.sock; do
    if [ ! -S "${dep}" ]; then
        echo "FAIL: downstream IPC socket ${dep} not found"
        exit 1
    fi
done

# --- DoIP 监听端口 ---
if command -v ss >/dev/null 2>&1; then
    if ! ss -ltn 2>/dev/null | grep -q ":13400"; then
        echo "FAIL: DoIP listener not found on port 13400"
        exit 1
    fi
fi

# --- 生命周期：重启后 socket 重建 ---
if ! systemctl restart tbox-diag.service; then
    echo "FAIL: could not restart tbox-diag.service"
    exit 1
fi

for i in $(seq 1 50); do
    if [ -S "${SOCKET_PATH}" ]; then
        break
    fi
    sleep 0.1
done

if [ ! -S "${SOCKET_PATH}" ]; then
    echo "FAIL: IPC socket ${SOCKET_PATH} not recreated after restart"
    exit 1
fi

if ! systemctl is-active --quiet tbox-diag.service; then
    echo "FAIL: tbox-diag.service not active after restart"
    exit 1
fi

# --- 优雅停止与 socket 清理 ---
systemctl stop tbox-diag.service

for i in $(seq 1 30); do
    if [ ! -e "${SOCKET_PATH}" ]; then
        break
    fi
    sleep 0.1
done

if [ -e "${SOCKET_PATH}" ]; then
    echo "WARN: IPC socket ${SOCKET_PATH} still exists after stop (may be cleaning up)"
fi

# 恢复冒烟后状态
systemctl start tbox-diag.service

echo "PASS: tbox-diag IPC/config smoke test (config, sockets, DoIP port, restart, stop, start)"
exit 0

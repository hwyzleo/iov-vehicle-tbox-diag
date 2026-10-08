#!/bin/bash
# Health check for tbox-diag service.
# Exit 0 = healthy, non-zero = unhealthy.
#
# DIAG 特有健康检查入口（BUILD manifests/services.yaml runtime.health_check）。

set -euo pipefail

BINARY="/usr/bin/tbox_diag"

# --- 正式发布产物存在性 ---
if [ ! -x "${BINARY}" ]; then
    echo "ERROR: tbox_diag binary not found at ${BINARY}"
    exit 1
fi

# --- 遗留单体产物检测 ---
# /opt/tbox/bin/diag 不由任何现行 install 规则生成；若 unit 仍指向该路径，
# 说明设备跑的是未经发布链路的旧产物（历史上曾因 libhwyz.so 缺少
# yaml-cpp DT_NEEDED 而 symbol lookup error → exit 127）。
if [ -e /opt/tbox/bin/diag ]; then
    echo "ERROR: legacy monolithic artifact /opt/tbox/bin/diag present;" \
         "device is not running the released diag-runtime component"
    exit 1
fi

# --- 动态符号完整性（有 ldd 时）---
# 任何 undefined symbol 都会在 exec 时变成 symbol lookup error，提前拦住。
if command -v ldd >/dev/null 2>&1; then
    if ldd -r "${BINARY}" 2>&1 | grep -q "undefined symbol"; then
        echo "ERROR: ${BINARY} has undefined symbols:"
        ldd -r "${BINARY}" 2>&1 | grep "undefined symbol" | head -10
        exit 1
    fi
fi

# --- systemd unit 活跃状态 ---
if ! systemctl is-active --quiet tbox-diag.service; then
    echo "ERROR: tbox-diag.service is not active"
    exit 1
fi

# --- IPC socket 存在性 ---
SOCKET_PATH="/tmp/tbox-diag.sock"
if [ ! -S "${SOCKET_PATH}" ]; then
    echo "ERROR: IPC socket ${SOCKET_PATH} not found"
    exit 1
fi

echo "OK: tbox-diag service is healthy (binary, symbols, service, socket)"
exit 0

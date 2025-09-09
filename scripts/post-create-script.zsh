#!/usr/bin/zsh

rm -rf .venv

python3 -m venv .venv
source .venv/bin/activate
pip install -U pip
pip install west
if [ ! -d ".west" ]; then
    west init --local app
fi
west update
west packages pip --install
west zephyr-export

# aarch64-zephyr-elf
# arc-zephyr-elf
# arc64-zephyr-elf
# arm-zephyr-eabi
# microblazeel-zephyr-elf
# mips-zephyr-elf
# nios2-zephyr-elf
# riscv64-zephyr-elf
# rx-zephyr-elf
# sparc-zephyr-elf
# x86_64-zephyr-elf
# xtensa-amd_acp_6_0_adsp_zephyr-elf
# xtensa-dc233c_zephyr-elf
# xtensa-espressif_esp32s2_zephyr-elf
# xtensa-espressif_esp32s3_zephyr-elf
# xtensa-espressif_esp32_zephyr-elf
# xtensa-intel_ace15_mtpm_zephyr-elf
# xtensa-intel_ace30_ptl_zephyr-elf
# xtensa-intel_tgl_adsp_zephyr-elf
# xtensa-mtk_mt818x_adsp_zephyr-elf
# xtensa-mtk_mt8195_adsp_zephyr-elf
# xtensa-mtk_mt8196_adsp_zephyr-elf
# xtensa-mtk_mt8365_adsp_zephyr-elf
# xtensa-nxp_imx8m_adsp_zephyr-elf
# xtensa-nxp_imx8ulp_adsp_zephyr-elf
# xtensa-nxp_imx_adsp_zephyr-elf
# xtensa-nxp_rt500_adsp_zephyr-elf
# xtensa-nxp_rt600_adsp_zephyr-elf
# xtensa-nxp_rt700_hifi1_zephyr-elf
# xtensa-nxp_rt700_hifi4_zephyr-elf
# xtensa-sample_controller32_zephyr-elf
# xtensa-sample_controller_zephyr-elf

west sdk install --toolchain x86_64-zephyr-elf xtensa-espressif_esp32s3_zephyr-elf
west blobs fetch hal_espressif

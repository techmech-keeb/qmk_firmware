VIA_ENABLE = yes
MCU = RP2040

# VIA設定
RAW_ENABLE = yes
DYNAMIC_KEYMAP_ENABLE = yes

# 最適化設定
LTO_ENABLE = no
OPT_FLAGS = -O2
EXTRAFLAGS += -fno-lto

# オーディオ設定
AUDIO_ENABLE = yes
AUDIO_DRIVER = pwm_hardware

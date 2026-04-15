if(SB_CONFIG_BOARD_NRF52840DONGLE_NRF52840 OR
   SB_CONFIG_BOARD_NRF52840DONGLE_NRF52840_BARE)
  set(PM_STATIC_YML_FILE
      ${CMAKE_CURRENT_LIST_DIR}/pm_static_nrf52840dongle_nrf52840.yml
      CACHE INTERNAL "")
elseif(SB_CONFIG_BOARD_PROMICRO_NRF52840_NRF52840)
  set(PM_STATIC_YML_FILE
      ${CMAKE_CURRENT_LIST_DIR}/pm_static_promicro_nrf52840_nrf52840.yml
      CACHE INTERNAL "")
endif()

# Keep board-specific MCUboot config in sysbuild/mcuboot/boards/<board>.conf.
# Zephyr auto-discovers that child-image board config, so appending the same file
# through mcuboot_EXTRA_CONF_FILE would merge it twice.

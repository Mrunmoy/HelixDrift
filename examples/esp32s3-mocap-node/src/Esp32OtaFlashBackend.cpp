#include "Esp32OtaFlashBackend.hpp"

namespace helix {

bool Esp32OtaFlashBackend::eraseSlot() {
    if (m_active) {
        closeSession();
    }

    m_partition = m_ops.getNextUpdatePartition(nullptr);
    if (!m_partition) {
        return false;
    }

    /* OTA_SIZE_UNKNOWN (0xFFFFFFFF) tells ESP-IDF to accept any image size. */
    constexpr size_t kOtaSizeUnknown = 0xFFFFFFFFu;
    const esp_err_t err = m_ops.begin(m_partition, kOtaSizeUnknown, &m_handle);
    m_active = (err == ESP_OK);
    return m_active;
}

bool Esp32OtaFlashBackend::writeChunk(uint32_t /*offset*/,
                                       const uint8_t* data,
                                       size_t         len) {
    if (!m_active) {
        return false;
    }
    return m_ops.write(m_handle, data, len) == ESP_OK;
}

bool Esp32OtaFlashBackend::setPendingUpgrade() {
    if (!m_active) {
        return false;
    }

    if (m_ops.end(m_handle) != ESP_OK) {
        m_active = false;
        return false;
    }

    const esp_err_t err = m_ops.setBootPartition(m_partition);
    m_active = false;
    return err == ESP_OK;
}

uint32_t Esp32OtaFlashBackend::slotSize() const {
    return m_slotSizeBytes;
}

void Esp32OtaFlashBackend::closeSession() {
    m_ops.end(m_handle);
    m_active    = false;
    m_partition = nullptr;
}

} // namespace helix

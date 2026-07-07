local M = {}

function M.read(pin, atten)
    return _esp_adc_read(pin, atten)
end

return M

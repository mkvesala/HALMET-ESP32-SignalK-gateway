## Description

Please include a summary of the change and which issue is fixed.

Fixes # (issue)

## Type of change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## How Has This Been Tested?

Please describe the tests that you ran to verify your changes.

- [ ] Tested on actual hardware
- [ ] ESP32 boots without errors; Serial shows `[VDO] ADS1115: OK`
- [ ] `propulsion.0.exhaustTemperature` updates in SignalK Data Browser
- [ ] `tanks.fuel.0.currentLevel` updates in SignalK Data Browser
- [ ] `tanks.fuel.0.capacity` appears as 0.4 on WebSocket connect
- [ ] Wi-Fi connects
- [ ] SignalK WebSocket works
- [ ] ESP-NOW broadcast transmits `HALMETEngineDelta` and `HALMETTankDelta` packets
- [ ] OTA updates successful

**Test Configuration**:
- ESP32 board: [e.g. ESP32 Dev Module]
- Arduino IDE: [e.g. 2.3.6]
- ESP32 package: [e.g. 3.3.5]

## Checklist:

- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have tested on actual hardware if possible
- [ ] I have acknowledged any AI assistance in code comments/PR description

## AI Assistance

- [ ] No AI tools used
- [ ] ChatGPT used for code generation/review
- [ ] Claude used for code generation/review
- [ ] GitHub Copilot used
- [ ] Other AI tools (specify): _____________

If AI was used, describe the prompts and how you verified the code:
```
(describe here)
```


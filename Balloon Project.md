# Initial Proposal

Here is your high-altitude weather balloon project documentation reformatted into a clean, highly scannable, and professional engineering proposal.

---

# **🎈 Mission To The Edge Of Space: Avionics & Flight Computer Proposal**

## **📌 Mission Statement**

To design, fabricate, and fly a custom embedded flight computer to **\~100,000 ft (30 km)** via a high-altitude weather balloon. The payload must operate autonomously for \~2 hours in near-vacuum, \-60°C conditions, transmit live telemetry via LoRa, capture Earth's curvature, and safely recover via a GPS-tracked parachute.

> **The Ultimate Deliverable:** Custom-designed avionics that flew to the edge of space and came back.

---

## **🏗️ System Architecture**

The payload consists of **three independent subsystems** housed within a single chassis:

1. **Flight Computer (Custom PCB):** STM32-based core that controls the mission state machine and telemetry.  
2. **Imaging Module:** Raspberry Pi Zero 2W \+ Camera, triggered externally by the Flight Computer.  
3. **Recovery System:** Parachute assembly integrated with independent GPS trackers.

### **System Block Diagram**

```
                     [Antenna]
                         │
[Battery] ──> [Power Reg] ──> [STM32 MCU] ──> [GPS (UART)]
                                │ ├─> [IMU (SPI)]
                                │ ├─> [Baro (I2C)]
                                │ ├─> [External Temp Probe]
                                │ ├─> [LoRa Radio (SPI)]
                                │ ├─> [microSD (SPI)]
                                │ └─> [Camera Trigger (GPIO)]
                                ▼
                         [Status LEDs]
```

*Note: The Pi Zero camera runs its own OS independently; the Flight Computer simply toggles a GPIO pin every N seconds to capture a photo.*

---

## **🎛️ Hardware Design (EE Work)**

**Specifications:** Custom 4-layer PCB, \~75×50 mm.

**Layer Stackup:** Signal — Ground — Power — Signal *(Solid ground plane required for clean RF return paths).*

### **Component Selection**

| Component | Part Number | Engineering Justification |
| :---- | :---- | :---- |
| **MCU** | STM32F411CEU6 | Cortex-M4 with FPU, abundant peripherals, excellent documentation. |
| **GPS** | u-blox MAX-M10S | **Critical:** Supports \>18 km altitude. Standard GPS locks out at \~60k ft due to COCOM export limits. |
| **Radio** | RFM95W LoRa (915 MHz) | Long-range, low-power, operates on license-free US ISM band. |
| **IMU** | ICM-42688-P | Ultra-low noise gyro/accelerometer for reliable attitude logging during float. |
| **Barometer** | BMP390 | Validated for high-altitude profiles and wide temperature swings. |
| **Ext. Temp** | TMP117 | External digital probe to accurately capture ambient air temperature. |
| **Storage** | SanDisk Industrial 32GB | Industrial grade; rated for wide operating temperatures (-40°C to 85°C). |
| **Power** | 4× Samsung INR18650-35E | 2S2P configuration. Lithium-ion chemistry holds voltage at \-30°C far better than LiPo. |
| **Regulator** | TPS63070 Buck-Boost | Dynamic 3.3V regulation as cell voltages drop rapidly in extreme cold. |
| **ESD Protection** | TVS Diodes | Placed on GPS, RF, and USB lines to survive static buildup on antennas. |

### **💡 Key Design Decisions to Defend in Interviews**

* **$\\lambda/4$ Monopole LoRa Antenna:** Measures exactly 8.2 cm at 915 MHz, paired with a dedicated PCB ground plane.  
* **Buck-Boost Topology:** Chosen over standard buck regulators because cold-induced battery voltage sag will drop below nominal requirements.  
* **Industrial SD Cards:** Standard consumer microSD cards heavily corrupt when exposed to temperatures below 0°C.

---

## **💻 Firmware Architecture (Embedded Work)**

### **Software Modules**

1. **HAL / Drivers:** Low-level configuration for GPIO, UART, SPI, I2C, ADC, Timers, and Watchdog.  
2. **Sensor Layer:** Drivers and data parsers for the IMU, Barometer, Temperature sensor, and GPS NMEA strings.  
3. **Storage:** FATFS integration featuring periodic data flushes and write-after-write verification.  
4. **Radio:** LoRa packet transmission featuring sequence numbering, CRC checks, and FEC utilizing the RadioLib library.  
5. **Telemetry Packetizer:** Packs all vital sensor vectors into a fixed-size, 48-byte transmission payload.  
6. **Camera Trigger:** Hardware GPIO toggle mapping to a strict 30-second interval during ascent.  
7. **Power Management:** Deep-sleep routine entries during low-dynamic float states.  
8. **Watchdog (IWDG):** Hardware watchdog to auto-recover and reboot the system from sensor brown-outs.

### **Mission State Machine**

```
[ PAD ] ──> Boot, device initialization, and cold GPS lock.
   │
[ ARMED ] ──> Triggered via physical button or magnetic switch.
   │
[ ASCENT ] ──> Positive altitude delta detected. Telemetry TX @ 1 Hz.
   │
[ FLOAT ] ──> Apogee reached. Telemetry drops to 0.2 Hz to preserve power.
   │
[ BURST ] ──> Sudden descent trigger detected (>50 m/s drop).
   │
[ DESCENT ] ──> Target falling. Telemetry TX updates to 0.5 Hz.
   │
[ LANDED ] ──> Altitude stable for >60 seconds. Enters ultra-low power beacon mode.
   │
[ BEACON ] ──> Emits GPS coordinates every 60 seconds indefinitely for recovery.
```

> **Firmware Elevator Pitch:** *"A real-time state machine driving an embedded avionics platform through a 2-hour mission in extreme environmental conditions, featuring autonomous failure-mode handling and a recoverable post-landing beacon."*

---

## **🛠️ Mechanical, Packaging & Logistics**

### **Payload Enclosure & Rigging**

* **Chassis:** 1" thick insulated foam box lined with reflective Mylar (Interior: \~6"×4"×3").  
* **Internal Layout:** Flight computer secured on central standoffs. Battery pack mounted flat at the base to maintain a low center of gravity. Pi Zero lens aligned with a small acrylic optical window.  
* **External Elements:** LoRa antenna oriented vertically on top; GPS patch antenna pointing zenith; temperature probe extended out the lateral wall.  
* **Recovery Hardware:** 30" nylon parachute, Nomex shock cords, and low-friction swivels attached directly to the balloon neck.  
* **Weight Profile:** **\< 4.0 lbs total mass.** This satisfies FAA Part 101 exemptions, requiring no formal FAA certifications.

### **Core Logistics Procurement**

* **Launch Site:** Central California Valley farmland. Safe distance (≥5 miles) from airports and major highways; predictable wind patterns.  
* **Gas Lift:** \~150 cu ft cylinder of Helium (Party City or local welding supply). Lift calculated via Archimedes' principle: $\\text{Payload Weight} \\times 1.5\\text{ positive lift}$.  
* **Balloon & Parachute:** Kaymont 1200g latex balloon paired with a 30" nylon recovery parachute from HABstore.

---

## **📅 12-Week Project Timeline**

```
💵 [W1-W3: Design & Fab] ──> 🛠️ [W4-W6: Bring-up & Test] ──> 🚀 [W7-W10: Integration & Test Launch] ──> 🛰️ [W11-W12: Main Mission]
```

### **Phase 1: Design & Fabrication**

* **Week 1:** Assign roles (PCB, Firmware, Ops). Start KiCad schematics. Order STM32 Nucleo-F411 dev board. Read FAA Part 101 rules.  
* **Week 2:** Complete schematics and clear Electrical Rules Check (ERC). Cross-verify Bill of Materials (BOM). Begin layout. Initialize basic UART/LED blinking on the Nucleo board.  
* **Week 3:** Complete 4-layer PCB layout. Export Gerbers and clear Design Rules Check (DRC). **Submit to JLCPCB by Wednesday.** Firmware lead completes IMU and Barometer test drivers.

### **Phase 2: Bring-Up & Local Testing**

* **Week 4:** Commercial parts arrive. Complete GPS NMEA parser and baseline LoRa TX code. Conduct a 5-mile ground-range line-of-sight radio test.  
* **Week 5:** PCB arrives. Hand-assemble via hot-plate reflow. Perform initial power-on and peripheral verification. *Note: Expecting 1 bodge wire / firmware workaround.*  
* **Week 6:** **Go/No-Go Board Respin Decision.** If unrecoverable, order a revised board Monday (8-day turnaround). If clear, port firmware from Nucleo to the custom PCB. Begin environmental freezer tests (-15°C).

### **Phase 3: Integration & Test Flights**

* **Week 7:** Finalize state machine integration. Test Pi Zero camera triggers. Validate SD card write resiliency by abruptly pulling power during active logs.  
* **Week 8:** Conduct an 24-hour continuous cold soak. Build the physical Mylar foam enclosure, perform a final mass check (\< 4 lbs), and finalize launch permissions.  
* **Week 9:** **Test Launch (Low Altitude).** Launch a scaled-down 300g balloon to \~25 km. This low-risk flight validates telemetry, tracking, and recovery workflows end-to-end.

### **Phase 4: Main Flight Operations & Closeout**

* **Week 10:** Process test flight feedback. File formal FAA NOTAM (Notice to Air Missions). Track daily trajectory predictions via predict.sondehub.org.  
* **Week 11:** 🚀 **MAIN LAUNCH WINDOW.** Deploy the 1200g balloon to 100k+ ft. Mobilize two chase vehicles tracking real-time LoRa telemetry to recover the downrange payload.  
* **Week 12:** Process sensor logs, create altitude/temperature telemetry plots, compile curvature photos into a technical report, and open-source the project repository on GitHub.

---

## **⚠️ Risk Mitigation Matrix**

| Identified Risk | Probability | Proactive Mitigation Strategy |
| :---- | :---- | :---- |
| **PCB Respin Required** | High (\~70%) | Buffer built into Week 6\. Firmware runs concurrently on generic Nucleo dev hardware to avoid losing time. |
| **Total Loss of Payload** | Moderate (\~20%) | Conduct a low-altitude test flight first. Implement redundant tracking systems (LoRa \+ standalone Iridium satellite backup). |
| **Cold-Induced System Crash** | Moderate | Environmental torture testing via dry-ice/freezer cycles prior to final assembly. |
| **GPS Altitude Lock Failure** | Low | Explicitly selected the u-blox MAX-M10S to bypass standard civilian COCOM limits. |
| **Battery Depletion** | Low | Implemented cold-hardy Lithium-ion 18650 cylindrical cells encased in insulated thermal foam. |

---

## **💰 Budget Breakdown**

| Budget Item Description | Minimum Flight Cost | Recommended "Moonshot" Flight Cost |
| :---- | :---- | :---- |
| Custom PCB Fabrication \+ Components (2 Flight Boards) | $220 | $220 |
| Kaymont 1200g Latex Balloon | $100 | $100 |
| Helium Gas Fill (\~150 cu ft) | $90 | $90 |
| Parachute, Nomex Shock Cord, & Rigging Lines | $40 | $40 |
| Raspberry Pi Zero 2W \+ Camera Module | $40 | $40 |
| u-blox MAX-M10S GPS Module | $35 | $35 |
| LoRa RF Modules \+ Antennas (x2 for Flight \+ Base Station) | $30 | $30 |
| 18650 Batteries \+ High-Retention Holder | $30 | $30 |
| Insulated Payload Structural Materials | $25 | $25 |
| STM32 Nucleo Development Board | $25 | $25 |
| Incidentals (Tape, Connectors, Helium Tank Deposit) | $50 | $50 |
| RockBLOCK 9603 Iridium SBD Satellite Tracker *(Stretch)* | — | $260 |
| Test Flight Consumables (300g Balloon \+ Helium) *(Stretch)* | — | $90 |
| **TOTAL PROJECT INVESTMENT** | **\~$775** | **\~$1,035** |

> 📌 **Funding Ask Strategy:** A tight **$500 bare-minimum budget** allows for a single high-risk flight with zero safety buffers. The optimal funding request is **$800** to ensure high recovery confidence through an initial test launch and secondary satellite tracking.

---

## **💼 Why This Secures Engineering Internships**

* **Mixed-Signal PCB Design with RF:** Demonstrates an understanding of impedance matching, ground planes, and high-frequency routing—skills highly valued by hardware teams.  
* **Ruggedized Embedded Firmware:** Proves you can write production-ready code that handles harsh environments, unexpected power drops, and state management.  
* **The Ultimate Talking Point:** *"My custom hardware survived a \-60°C near-vacuum environment at 30 km altitude."* It mimics NASA-aligned workflows and proves you can take a complex engineering project from schematic to flight.

## **\[IMPORTANT\] Initial Revisions ft. Gemini**

This is already an exceptionally strong proposal. It reads like a capstone project or a professional aerospace prototype. However, if you are presenting this to a review board for funding, or using it to impress an interviewer, there are a few technical vulnerabilities, logistical realities, and firmware edge cases you should tweak.\]

Here are the specific optimizations I would make across your design:

### **1\. Hardware & Electrical Tweaks**

* **The Battery Chemistry Choice:** You noted that Lithium-ion (18650) holds voltage better than LiPo. While true compared to *some* cheap LiPos, standard Li-ion chemistries still suffer massive capacity drops (up to 40–50%) and voltage sag at **\-60°C**. For aerospace applications, **Lithium Iron Phosphate ($LiFePO\_4$)** or **Lithium Manganese Dioxide ($LiMnO\_2$ primary cells like Energizer Ultimate Lithiums)** are the gold standards because their internal resistance doesn’t skyrocket as violently in extreme cold.  
* **GPS Antenna Orientation:** You specified a "GPS patch antenna pointing zenith." Patch antennas require a massive, stable ground plane to function well and can lose lock if the payload spins or tilts violently (which weather balloons do during ascent and burst). Consider upgrading to a **helical GPS antenna**—they have an omnidirectional radiation pattern that handles payload sway much better.

### **2\. Firmware & Edge Case Tweaks**

* **The "Burst" State Logic:** Your state machine detects a burst via a sudden descent trigger ($\>50\\text{ m/s}$ drop). Be careful here: right at the moment of burst, the air is so thin that the payload will virtually free-fall, easily exceeding $50\\text{ m/s}$. However, as it hits thicker air lower down, the parachute will slow it down significantly. Ensure your state machine doesn't get confused if the descent speed drops back *below* your threshold later in the flight.  
* **LoRa Duty Cycle & Bandwidth:** You have telemetry firing at 1 Hz during ascent. Depending on your LoRa Spreading Factor (SF) and bandwidth settings (needed to achieve that 5+ mile line-of-sight range), a 48-byte payload might take several hundred milliseconds to transmit. A 1 Hz rate might violate local ISM band duty cycle regulations or saturate your airtime. I'd recommend throttling the baseline ascent telemetry to **0.25 Hz or 0.5 Hz (every 2 to 4 seconds)** to ensure the transmitter doesn't overheat and you stay legally compliant.

### **3\. Logistical & Budget Realities**

* **Helium Pricing:** You budgeted $90 for 150 cu ft of Helium. Unless you have a specific hookup via a university lab, industrial Helium prices have spiked wildly over the last few years. 150 cu ft of pure lab/welding-grade Helium is more likely to run you **$150 to $250** today. (Party City gas is often a Helium-Air mix and won't give you the lift you calculated via Archimedes' principle).  
* **The "Funding Ask" Math:** Your budget total says **Minimum: \~$775**, but your Funding Ask paragraph says *"A tight $500 bare-minimum budget..."* Make sure those numbers match perfectly so a sharp-eyed interviewer or professor doesn't catch the discrepancy. Change the text to reflect that your *absolute bare minimum* is \~$775.

### **4\. Re-shaping for the Interview (The Pitch)**

* **Add a "Thermal Management Strategy":** In a near-vacuum, there is no air to conduct heat. Ironically, components like your buck-boost regulator and the Pi Zero can *overheat* if they run hard, even though the outside air is \-60°C. Mentioning **"Vacuum Thermal Management via internal copper thermal mass dissipation"** as a design challenge you solved will absolutely blow a hardware interviewer away.

Would you like help re-writing the specific sections (like the state machine logic or the budget text) to seamlessly bake these upgrades into your original layout?

---

Here is the fully revised engineering proposal. The technical vulnerabilities, budget math, firmware edge cases, and thermal challenges have been seamlessly integrated to make this airtight for funding boards and interviewers.

---

# 

# **\[REDO 1\] 🎈 Mission to the Edge of Space: Avionics & Flight Computer Proposal**

## **📌 Mission Statement**

To design, fabricate, and fly a custom embedded flight computer to **\~100,000 ft (30 km)** via a high-altitude weather balloon. The payload must operate autonomously for **\~2 hours** in near-vacuum, **\-60°C** conditions, transmit live telemetry via LoRa, capture Earth's curvature, and safely recover via a GPS-tracked parachute.

> **The Ultimate Deliverable:** Custom-designed avionics that flew to the edge of space and came back.

---

## **🏗️ System Architecture**

The payload consists of three independent subsystems housed within a single chassis:

1. **Flight Computer (Custom PCB):** An STM32-based core that controls the mission state machine and telemetry.  
2. **Imaging Module:** A Raspberry Pi Zero 2W \+ Camera, triggered externally by the Flight Computer.  
3. **Recovery System:** A parachute assembly integrated with independent GPS trackers.

### **System Block Diagram**

Plaintext

```
                     [Omni Antennas]
                            │
[Battery Pack] ──> [Power Reg] ──> [STM32 MCU] ──> [Helical GPS (UART)]
                                    │ ├─> [IMU (SPI)]
                                    │ ├─> [Baro (I2C)]
                                    │ ├─> [External Temp Probe]
                                    │ ├─> [LoRa Radio (SPI)]
                                    │ ├─> [microSD (SPI)]
                                    │ └─> [Camera Trigger (GPIO)]
                                    ▼
                             [Status LEDs]
```

*Note: The Pi Zero camera runs its own OS independently; the Flight Computer simply toggles a GPIO pin every 30 seconds to capture a photo.*

---

## **🎛️ Hardware Design (EE Work)**

* **Specifications:** Custom 4-layer PCB, \~75×50 mm.  
* **Layer Stackup:** Signal — Ground — Power — Signal *(Solid ground plane required for clean RF return paths).*

### **Component Selection**

| Component | Part Number | Engineering Justification |
| :---- | :---- | :---- |
| **MCU** | STM32F411CEU6 | Cortex-M4 with FPU, abundant peripherals, excellent documentation. |
| **GPS** | u-blox MAX-M10S | **Critical:** Supports \>18 km altitude. Standard GPS locks out at \~60k ft due to COCOM export limits. |
| **GPS Antenna** | Helical Omni-Antenna | Replaced patch antenna to maintain satellite lock during violent payload spin/sway. |
| **Radio** | RFM95W LoRa (915 MHz) | Long-range, low-power, operates on license-free US ISM band. |
| **IMU** | ICM-42688-P | Ultra-low noise gyro/accelerometer for reliable attitude logging during float. |
| **Barometer** | BMP390 | Validated for high-altitude profiles and wide temperature swings. |
| **Ext. Temp** | TMP117 | External digital probe to accurately capture ambient air temperature. |
| **Storage** | SanDisk Industrial 32GB | Industrial grade; rated for wide operating temperatures (-40°C to 85°C). |
| **Power** | 4× Energizer L91 ($LiMnO\_2$) no  | Primary Lithium cells. Outperforms LiPo/Li-ion at \-60°C with negligible internal resistance spikes. |
| **Regulator** | TPS63070 Buck-Boost | Dynamic 3.3V regulation to handle low-temperature battery voltage sag. |
| **ESD Protection** | TVS Diodes | Placed on GPS, RF, and USB lines to survive static buildup on antennas. |

### **💡 Key Design Decisions to Defend in Interviews**

* **Vacuum Thermal Management:** In a near-vacuum, there is no air to conduct heat away from components. The PCB utilizes expanded copper planes and heavy thermal vias to sink heat away from the buck-boost regulator and the Pi Zero, turning the board itself into a thermal mass to prevent overheating.  
* **$\\lambda/4$ Monopole LoRa Antenna:** Measures exactly 8.2 cm at 915 MHz, paired with a dedicated PCB ground plane.  
* **Primary Lithium Chemistry ($LiMnO\_2$):** Chosen over standard Lithium-ion or LiPo cells because its chemistry maintains voltage stability and retains capacity down to \-60°C without requiring heavy, power-hungry internal heaters.  
* **Industrial SD Cards:** Standard consumer microSD cards heavily corrupt when exposed to temperatures below 0°C.

---

## **💻 Firmware Architecture (Embedded Work)**

### **Software Modules**

* **HAL / Drivers:** Low-level configuration for GPIO, UART, SPI, I2C, ADC, Timers, and Watchdog.  
* **Sensor Layer:** Drivers and data parsers for the IMU, Barometer, Temperature sensor, and GPS NMEA strings.  
* **Storage:** FATFS integration featuring periodic data flushes and write-after-write verification.  
* **Radio:** LoRa packet transmission featuring sequence numbering, CRC checks, and FEC utilizing the RadioLib library.  
* **Telemetry Packetizer:** Packs all vital sensor vectors into a fixed-size, 48-byte transmission payload.  
* **Camera Trigger:** Hardware GPIO toggle mapping to a strict 30-second interval during ascent.  
* **Power Management:** Deep-sleep routine entries during low-dynamic float states.  
* **Watchdog (IWDG):** Hardware watchdog to auto-recover and reboot the system from sensor brown-outs.

### **Mission State Machine**

Plaintext

```
[ PAD ] ──> Boot, device initialization, and cold GPS lock.
   │
[ ARMED ] ──> Triggered via physical button or magnetic switch.
   │
[ ASCENT ] ──> Positive altitude delta. Telemetry TX @ 0.5 Hz (Optimized for RF airtime regulations).
   │
[ FLOAT ] ──> Apogee reached. Telemetry drops to 0.2 Hz to preserve power.
   │
[ BURST ] ──> Sudden descent trigger detected (>50 m/s drop). Latches state to prevent resetting in thick air.
   │
[ DESCENT ] ──> Target falling. Telemetry TX updates to 0.5 Hz.
   │
[ LANDED ] ──> Altitude stable for >60 seconds. Enters ultra-low power beacon mode.
   │
[ BEACON ] ──> Emits GPS coordinates every 60 seconds indefinitely for recovery.
```

> **Firmware Elevator Pitch:** *"A real-time state machine driving an embedded avionics platform through a 2-hour mission in extreme environmental conditions, featuring autonomous failure-mode handling, RF duty-cycle compliance, and a recoverable post-landing beacon."*

---

## **🛠️ Mechanical, Packaging & Logistics**

### **Payload Enclosure & Rigging**

* **Chassis:** 1" thick insulated foam box lined with reflective Mylar (Interior: \~6"×4"×3").  
* **Internal Layout:** Flight computer secured on central standoffs. Battery pack mounted flat at the base to maintain a low center of gravity. Pi Zero lens aligned with a small acrylic optical window.  
* **External Elements:** Omnidirectional LoRa antenna oriented vertically on top; helical GPS antenna pointing zenith; temperature probe extended out the lateral wall.  
* **Recovery Hardware:** 30" nylon parachute, Nomex shock cords, and low-friction swivels attached directly to the balloon neck.  
* **Weight Profile:** \< 4.0 lbs total mass. This satisfies FAA Part 101 exemptions, requiring no formal FAA certifications.

### **Core Logistics & Procurement**

* **Launch Site:** Central California Valley farmland. Safe distance (≥5 miles) from airports and major highways; predictable wind patterns.  
* **Gas Lift:** \~150 cu ft cylinder of pure industrial-grade Helium (sourced via local welding supply). Lift calculated via Archimedes' principle:  
  $$\\text{Payload Weight} \\times 1.5\\text{ positive lift}$$  
* **Balloon & Parachute:** Kaymont 1200g latex balloon paired with a 30" nylon recovery parachute from HABstore.

---

## **📅 12-Week Project Timeline**

Plaintext

```
💵 [W1-W3: Design & Fab] ──> 🛠️ [W4-W6: Bring-up & Test] ──> 🚀 [W7-W10: Integration & Test Launch] ──> 🛰️ [W11-W12: Main Mission]
```

### **Phase 1: Design & Fabrication**

* **Week 1:** Assign roles (PCB, Firmware, Ops). Start KiCad schematics. Order STM32 Nucleo-F411 dev board. Read FAA Part 101 rules.  
* **Week 2:** Complete schematics and clear Electrical Rules Check (ERC). Cross-verify Bill of Materials (BOM). Begin layout. Initialize basic UART/LED blinking on the Nucleo board.  
* **Week 3:** Complete 4-layer PCB layout with dedicated internal thermal copper planes. Export Gerbers and clear Design Rules Check (DRC). Submit to JLCPCB. Firmware lead completes IMU and Barometer test drivers.

### **Phase 2: Bring-Up & Local Testing**

* **Week 4:** Commercial parts arrive. Complete GPS NMEA parser and baseline LoRa TX code. Conduct a 5-mile ground-range line-of-sight radio test.  
* **Week 5:** PCB arrives. Hand-assemble via hot-plate reflow. Perform initial power-on and peripheral verification. *Note: Expecting 1 bodge wire / firmware workaround.*  
* **Week 6:** Go/No-Go Board Respin Decision. If unrecoverable, order a revised board Monday (8-day turnaround). If clear, port firmware from Nucleo to the custom PCB. Begin environmental freezer tests (-15°C).

### **Phase 3: Integration & Test Flights**

* **Week 7:** Finalize state machine integration. Test Pi Zero camera triggers. Validate SD card write resiliency by abruptly pulling power during active logs.  
* **Week 8:** Conduct an 24-hour continuous cold soak. Build the physical Mylar foam enclosure, perform a final mass check (\< 4 lbs), and finalize launch permissions.  
* **Week 9:** **Test Launch (Low Altitude).** Launch a scaled-down 300g balloon to \~25 km. This low-risk flight validates telemetry, tracking, and recovery workflows end-to-end.

### **Phase 4: Main Flight Operations & Closeout**

* **Week 10:** Process test flight feedback. File formal FAA NOTAM (Notice to Air Missions). Track daily trajectory predictions via predict.sondehub.org.  
* **Week 11:** 🚀 **MAIN LAUNCH WINDOW.** Deploy the 1200g balloon to 100k+ ft. Mobilize two chase vehicles tracking real-time LoRa telemetry to recover the downrange payload.  
* **Week 12:** Process sensor logs, create altitude/temperature telemetry plots, compile curvature photos into a technical report, and open-source the project repository on GitHub.

---

## **⚠️ Risk Mitigation Matrix**

| Identified Risk | Probability | Proactive Mitigation Strategy |
| :---- | :---- | :---- |
| **PCB Respin Required** | High (\~70%) | Buffer built into Week 6\. Firmware runs concurrently on generic Nucleo dev hardware to avoid losing time. |
| **Total Loss of Payload** | Moderate (\~20%) | Conduct a low-altitude test flight first. Implement redundant tracking systems (LoRa \+ standalone Iridium satellite backup). |
| **Vacuum Overheating** | Moderate | Embedded deep copper thermal planes within the 4-layer stackup to distribute heat away from switching regulators without air conduction. |
| **GPS Altitude Lock Failure** | Low | Explicitly selected the u-blox MAX-M10S to bypass standard civilian COCOM limits. |
| **Battery Voltage Sag** | Low | Abandoned Li-ion/LiPo for primary Lithium $LiMnO\_2$ cells that remain operational at extreme negatives. |

---

## **💰 Budget Breakdown**

| Budget Item Description | Minimum Flight Cost | Recommended "Moonshot" Cost |
| :---- | :---- | :---- |
| Custom PCB Fabrication \+ Components (2 Flight Boards) | $220 | $220 |
| Kaymont 1200g Latex Balloon | $100 | $100 |
| Welding-Grade Helium Gas Fill (\~150 cu ft Cylinder) | $180 | $180 |
| Parachute, Nomex Shock Cord, & Rigging Lines | $40 | $40 |
| Raspberry Pi Zero 2W \+ Camera Module | $40 | $40 |
| u-blox MAX-M10S GPS Module | $35 | $35 |
| LoRa RF Modules \+ Antennas (x2 for Flight \+ Base Station) | $30 | $30 |
| Energizer Lithium Primary Cells ($LiMnO\_2$) \+ Holder | $20 | $20 |
| Insulated Payload Structural Materials | $25 | $25 |
| STM32 Nucleo Development Board | $25 | $25 |
| Incidentals (Tape, Connectors, Helium Tank Deposit) | $60 | $60 |
| *RockBLOCK 9603 Iridium SBD Satellite Tracker (Stretch)* | — | $260 |
| *Test Flight Consumables (300g Balloon \+ Helium) (Stretch)* | — | $150 |
| **TOTAL PROJECT INVESTMENT** | **\~$775** | **\~$1,180** |

📌 **Funding Ask Strategy:** A tight $775 budget allows for a baseline flight with standard tracking. The **optimal funding request is $1,180**, which introduces critical aerospace-grade margins through a low-altitude test flight and secondary satellite tracking backup.

---

## **💼 Why This Secures Engineering Internships**

* **Mixed-Signal PCB Design with RF:** Demonstrates an understanding of impedance matching, ground planes, and high-frequency routing—skills highly valued by hardware teams.  
* **Thermal and Environmental Systems Engineering:** Proves you can design for anomalous operating conditions like zero-convection thermal dissipation and sub-zero battery environments.  
* **Ruggedized Embedded Firmware:** Proves you can write production-ready code that handles harsh environments, unexpected power drops, and state management.  
* **The Ultimate Talking Point:** *"My custom hardware survived a \-60°C near-vacuum environment at 30 km altitude."* It mimics NASA-aligned workflows and proves you can take a complex engineering project from schematic to flight.

# Revisions

## **HAB Flight Computer — Final Bill of Materials v3**

newAdded  
changedModified  
optionalCan cut if over budget  
SSI coveredNot from your budget  
5V main rail

TPS63070 (buck-boost from battery)

Powers Pi Zero directly \+ feeds lower rails

3.3V logic rail

AP2112K-3.3 (LDO from 5V)

STM32, IMU, LoRa, APRS, temp, SD

1.8V GPS rail

XC6206P182MR (LDO from 3.3V)

MAX-M10S GPS module only

PCB / Electronics$157

| Part | Part number | Notes | Qty | Unit | Total |
| :---- | :---- | :---- | :---: | :---- | ----: |
| MCU Cortex-M4, main brain | STM32F411CEU6 | Bare IC, 3.3V rail | 1 | $10 | $10 |
| Crystal 8MHz External clock for STM32 | ABM8-8.000MHZ | Cold-stable oscillator reference | 1 | $1 | $1 |
| GPS Module \>18km altitude bypass | u-blox MAX-M10S | 1.8V rail — cannot connect to 3.3V directly | 1 | $35 | $35 |
| GPS Patch Antenna Passive ceramic patch | Taoglas FXP73 | Pairs with MAX-M10S via U.FL, faces up | 1 | $4 | $4 |
| GPS Level Shifter 3.3V ↔ 1.8V UART bridge | TXS0102new | STM32 UART is 3.3V logic, GPS UART is 1.8V — need translation | 1 | $0.80 | $0.80 |
| LoRa Module (flight) 915 MHz primary downlink | RFM95W | 3.3V rail, SPI to STM32 | 1 | $10 | $10 |
| LoRa Module (ground) 915 MHz ground station RX | RFM95W | Connects to Nucleo dev board | 1 | $10 | $10 |
| APRS Radio Module VHF 144.390 MHz backup | Dorji DRA818V | 3.3V rail, UART config \+ DAC audio in | 1 | $8 | $8 |
| VHF Low-Pass Filter Harmonic suppression | Mini-Circuits or discrete | Required by FCC for ham TX | 1 | $3 | $3 |
| IMU Sensor 6-axis gyro \+ accel | ICM-42688-P | Bare IC, SPI, 3.3V rail | 1 | $6 | $6 |
| Ext. Temp Probe Ambient digital sensor | TMP117 | ±0.1°C, I2C, 3.3V railoptional | 1 | $4 | $4 |
| Buck-Boost 5V Main power rail | TPS63070changed | Now set to 5V output (resistor change only) — feeds 3.3V LDO \+ Pi Zero directly | 1 | $4 | $4 |
| LDO 3.3V Logic rail from 5V | AP2112K-3.3changed | Was \-5.0 variant. Now takes 5V in → 3.3V out. Input safely within 6V max rating. | 1 | $0.50 | $0.50 |
|  |  |  |  |  |  |
| Polyfuse 2A Resettable fuse | MF-MSMF200 | Main power line, trips before battery vents | 1 | $0.30 | $0.30 |
| Reverse Polarity MOSFET P-ch at power input | DMG2305UX | Protects all ICs if battery reversed | 1 | $0.30 | $0.30 |
| Bulk Input Capacitor 100µF at battery input | Tantalum or electrolytic | Smooths LoRa TX spikes, prevents STM32 resets | 2 | $0.50 | $1 |
| Power Switch SPDT External on/off on box wall | Any panel mount | Power up on pad without opening box | 1 | $1.50 | $1.50 |
| Power LED \+ resistor 3.3V rail indicator | Generic | Visual confirmation board is live | 1 | $0.20 | $0.20 |
| microSD Holder Telemetry log only | Molex 104031 | SPI to STM32, no photos stored here | 1 | $1 | $1 |
| SWD Debug Header 5-pin 1.27mm | Generic | Mandatory for firmware flashing | 1 | $1 | $1 |
| PCB Fabrication 4-layer, JLCPCB pool | Custom Design | 5 copies per order, plan 1 respin | 2 | $15 | $30 |
| Passives & Protection Caps, resistors, TVS, ferrites | Various (LCSC) | Includes \~20 decoupling caps, audio divider resistors, crystal load caps | 1 | $25 | $25 |

Embedded / Dev$100

| Part | Part number | Notes | Qty | Unit | Total |
| :---- | :---- | :---- | :---: | :---- | ----: |
| Dev Board Firmware proto \+ ground station RX | STM32 Nucleo-F411RE | Doubles as ground station during flight — connects to RFM95W \#2 | 1 | $25 | $25 |
| ~~Imaging Computer Linux, camera stack, SSDV encode~~ | ~~Raspberry Pi Zero 2W~~ | ~~5V direct from main rail, GPIO trigger \+ UART back to STM32~~ | ~~1~~ | ~~$15~~ | ~~$15~~ |
| ~~Camera Module 12MP, autofocus, IMX708~~ | ~~Pi Camera Module 3~~ | ~~CSI-2 ribbon to Pi Zero, full-res saved \+ downscaled for SSDV~~ | ~~1~~ | ~~$25~~ | ~~$25~~ |
| Action Camera Cinematic post-recovery footage | Runcam 2 / Thumb Pro | External velcro mount, no electrical connection | 1 | $45 | $45 |
| ~~Storage — Pi photos Pi Zero boot \+ image storage~~ | ~~SanDisk Industrial 32GB~~ | ~~Goes in Pi Zero built-in SD slot~~ | ~~1~~ | ~~$15~~ | ~~$15~~ |
| Storage — STM32 log Telemetry only, no photos | SanDisk Industrial 8GB | Goes in PCB microSD holder via SPI | 1 | $10 | $10 |
| SSDV Library Image packetizer | github.com/fsphil/ssdv | Open source, runs on Pi Zero | — | $0 | $0 |
| Firmware Toolchain IDE \+ libraries | STM32CubeIDE / OSS | Free | — | $0 | $0 |
| Ham License Exam Required for APRS TX | ARRL / local club | One weekend study, valid 10 years | 1 | $15 | $15 |

Mechanical / Ops SSI covered$347

| Part | Part number | Notes | Qty | Unit | Total |
| :---- | :---- | :---- | :---: | :---- | ----: |
| Primary Flight Balloon | Kaymont 1200g | Standard HAB, correct for 100k ft target | 1 | $100 | $100 |
| Recovery Parachute | HABstore / Apogee 30" | Right size for sub-4lb payload | 1 | $30 | $30 |
| Rigging Accessories | Various | Nomex cord, swivels, flight line | 1 | $10 | $10 |
| Helium Gas Fill | Local Welding Supply | \~150 cu ft, get slightly extra | 1 | $90 | $90 |
| Flight Batteries |  | pre-protected 2S Li-ion pack | 4 |  |  |
| Battery Chassis | 2S2P 18650 Holder | High-retention cell holder | 1 | $6 | $6 |
| Enclosure Materials | Local hardware store | 1" foam \+ Mylar lining | 1 | $25 | $25 |
| Antennas 915MHz | Amazon / Digikey | One on payload, one at ground station | 2 | $5 | $10 |
| VHF Antenna 144MHz | Any 144MHz whip | Payload only — APRS TX | 1 | $5 | $5 |
| Hand Warmers | Any grocery store | Inside payload box, battery insurance | 4 | $0.50 | $2 |
| Operational Misc. | Various | Tape, zip ties, NOTAM filing, gas deposits | 1 | $50 | $50 |

Grand total

Electronics only (PCB \+ Embedded): **$257** — within your $200–300 budget

Power rails verified · all voltages within spec · 3 new parts added · 2 parts changed  
$604  
$347 SSI \+ $257 your budget

STM32F411CEU6

- runs at 100 megahertz  
- no linux (operating system)

**8MHz Crystal ( )** 

- the crystal is the stable temperature compensated clock reference  
- without it, the stm32’s internal oscillator would drift significantly with temperature \-60 degrees celsius (drift by several percents)

u-blox MAX-M10S

- GPS receiver module   
  - Talks to satellites and output position data over UART to STM32

GPS Patch Antenna (**Taoglas FXP73**)

- the MAX-M10S has a U.FL connector  
  - this antenna plugs into it 

RFM95W

- the LoRa radio transceiver module  
- SPI  
- RF signal transmitted at 915 MHz  
- Second one for GND 

**Dorji DRA818V** 

- VHF FM transceiver module covering 134-174 MHz  
  - **specifically 144.390 MHz for APRS**

ICM-42688-P

- this is IMU  
- measures rotational rate, linear acceleration  
- detects bursts when balloon pops at altitude (it’ll show on accelerometer) 

TMP117

- its a temperature sensor  
- can measure \+- 0.1 degrees C \-40 degrees C to 150 degrees C

TPS63070

- its a buck boost  
- For 5V rail


AP2112K-3.3 (add to BOM)

- Creates the 3.3V rail

# MF-MSMF250/16X-2

- Protects against huge current spikes for battery

DMG2305UX

- Protects against us being retarded and plugging the battery in the wrong way

XC6206P182MR (LDO)

- Creates the 1.8V rail for MAX-M10S module

microSD Card Holder

- connects to STM32 over SPI  
- log sensor data, gps coordinates, and flight events

\*\*\*Two SD cards though — only one custom holder for STM32 for data, the other holder is built in Pi Zero

SWD Debug Header

- programming and debugging interface  
- using this thing for programming on STM32 (uploads code)

**Raspberry Pi Zero 2W** 

- has camera interface  
- job is to receive a trigger signal from STM32 to tell when to capture photo using pi camera  
- interprets the images  
- sends data back to STM32

**Pi Camera Module 3** 

- takes photos

TXS0102

- Allows us to communicate between GPS module (1.8V) and the STM (tx pin) (STM runs at 3.3V)

Runcam

- its own camera   
- probably GoPro

**Communication Protocols**  
SPI: IMU, LoRa module, SD card   
I2C: temperature sensor  
UART: GPS and HDRS 

**PCB Architecture**  
Two boards: flight computer and Pi-Zero

- Pi-Zero is premade \- we’re making only one board

	

**GND Station**  
Laptop → USB cable → STM32 Nucleo → RFM95W module → 915MHz antenna 

**3 dif types antenna**   
915MHz – 2 – GND and Payload  
144MHz – 1 – for APRS  
Taoglas FXP73 GPS patch – 1 – for GPS

General Format: 

### **One Custom PCB, Everything Hanging Off It**

\*\*NO INTERNAL CLOCK

**On the PCB itself (soldered directly):**

* STM32F411 — the brain  
  * Pin count? (32 vs 48 vs 64\)  
* ICM-42688-P — IMU  
* TMP117 — temp sensor  
* TPS63070 — 3.3V regulator  
* AP2112K — 5V regulator  
* microSD holder — telemetry log  
* SWD header — firmware flashing  
* Crystal 8MHz — clock reference  
* All passives (caps, resistors, ferrites) \[book?\]

**Modules that sit on or connect to the PCB:**

* RFM95W → via U.FL coax → 915MHz whip antenna (exits box wall) transmitter  
* DRA818V → via U.FL coax → 144MHz whip antenna (exits box wall) transmitter  
* MAX-M10S → via U.FL coax → Taoglas FXP73 GPS patch (on top of box) receiver

**External boards connected via wires/JST:**

* Raspberry Pi Zero 2W → connected via 5V power \+ UART \+ GPIO trigger  
  * Pi Camera Module 3 → plugs into Pi via CSI-2 ribbon cable, lens faces out through foam cutout

**Completely standalone, no electrical connection:**

* Runcam — just velcroed to the outside of the box

---

### **Every Component — What It Actually Does**

**STM32F411CEU6** This is the brain. A 32-bit ARM Cortex-M4 microcontroller running at up to 100MHz with a hardware floating point unit (FPU). It runs your firmware — the flight state machine, sensor polling loop, radio packet assembly, GPS parsing, everything. It has no operating system, no Linux, no scheduler you didn't write. It just executes your code instruction by instruction at 100 million cycles per second. The FPU matters because barometric altitude calculations and GPS coordinate math involve floating point numbers — without hardware FPU you'd do that in slow software emulation.

**8MHz Crystal (ABM8)** The STM32 has an internal oscillator but it drifts significantly with temperature — at \-60°C it could be off by several percent. That sounds small but it means UART baud rates drift, SPI timing shifts, and your timers become inaccurate. The external crystal gives you a stable, temperature-compensated clock reference. The STM32 uses this as its phase-locked loop (PLL) input and multiplies it up to 100MHz internally. Two small load capacitors (typically 12–18pF, check crystal datasheet) sit on each crystal pin to ground — without these the crystal won't oscillate.

**u-blox MAX-M10S** A GNSS (GPS \+ GLONASS \+ Galileo) receiver module. Internally it has a dedicated RF front end, a SAW filter to reject out-of-band interference, a low-noise amplifier, and a baseband processor that tracks satellite signals and computes position. The key reason you chose this specific module is the COCOM limit bypass — most GPS chips hard-lock and output nothing above 18km altitude or above 515 m/s velocity, because of export control regulations designed to prevent use in ballistic missiles. The MAX-M10S is pre-certified for high-altitude balloon use and keeps outputting valid position data all the way to 80km. At 30km it's perfectly in range.

**GPS Patch Antenna (Taoglas FXP73)** A passive ceramic patch antenna that receives 1575.42 MHz L1 GPS signals. The MAX-M10S module has a U.FL connector — this antenna plugs into it. It needs to face the sky (upward) with a clear view of the hemisphere. Ceramic patch antennas are directional — they receive well from the upper hemisphere and poorly from below, which is exactly what you want since the satellites are above you.

**RFM95W** A LoRa radio transceiver module based on the Semtech SX1276 chip. It takes a data packet from the STM32 via SPI and converts it to a chirp spread spectrum (CSS) RF signal transmitted at 915MHz. CSS modulation is the magic — it spreads the signal across a wide bandwidth in a chirp pattern that can be decoded even when the signal is 20dB below the noise floor. This is why LoRa works at 100km+ with just 100mW of transmit power. The module includes the SX1276 chip, matching network, crystal, and U.FL connector — all the RF complexity is handled inside.

**Dorji DRA818V** A VHF FM transceiver module covering 134–174 MHz, used here specifically on 144.390 MHz for APRS. Internally it's a complete FM radio on a chip — it takes audio-frequency FSK data from the STM32, modulates it onto the carrier, and transmits. For APRS your STM32 generates 1200 baud AFSK (Audio Frequency Shift Keying) tones — 1200Hz and 2200Hz audio — and feeds them into the DRA818V's audio input pin. The module does the FM modulation and power amplification. Output power is about 1W, significantly more than the RFM95W's 100mW, but it uses a conventional modulation scheme that has much lower receiver sensitivity than LoRa.

**ICM-42688-P** A 6-axis inertial measurement unit — 3-axis gyroscope plus 3-axis accelerometer on a single tiny die. The gyro measures rotational rate (degrees per second) around each axis. The accelerometer measures linear acceleration (g-forces) along each axis. On a balloon this tells you spin rate, pendulum swing, and orientation. More critically it detects burst — when the balloon pops at altitude, the payload goes from gentle upward acceleration to sudden freefall, which shows up instantly on the accelerometer. This can trigger your flight state machine to transition from "ascending" to "descending" faster than waiting for GPS altitude to confirm. It communicates over SPI at up to 24MHz — very fast because you might want IMU data at 1000Hz for high-rate logging.

**BMP390** A MEMS barometric pressure sensor. Inside the chip is a tiny silicon membrane that flexes under atmospheric pressure — this flex is measured capacitively and converted to a pressure reading. Your firmware converts pressure to altitude using the barometric formula. At sea level pressure is \~1013 hPa. At 30km it's \~12 hPa. The BMP390 is accurate to about ±0.5 hPa which corresponds to roughly ±4m altitude resolution at sea level, degrading at altitude. It also has a temperature sensor inside for pressure compensation. Important note: the BMP390 goes out of spec below \~300 hPa (\~9km altitude) — below that pressure floor the reading clamps. So you use it for precise low-altitude readings (launch, landing, the first 9km) and rely on GPS altitude above that.

**TMP117** A precision digital temperature sensor accurate to ±0.1°C across \-40°C to \+150°C. Unlike the BMP390's internal temperature sensor which measures the chip's own die temperature, the TMP117 can be mounted externally — attached to a wire that pokes through the foam wall — to measure true ambient outside air temperature. At 30km altitude you'll see \-60°C to \-70°C. This is scientifically interesting data and also useful for monitoring battery temperature inside the box. Communicates over I2C.

**TPS63070** A buck-boost DC-DC switching regulator. "Buck-boost" means it can both step voltage down (buck) and step it up (boost), maintaining a stable output regardless of whether input is above or below the output. Your 2S2P battery pack outputs 6.0–8.4V across its discharge curve. The TPS63070 takes that variable input and outputs a rock-solid 3.3V at up to 2A. If you used a simple linear regulator (LDO) instead, it would work when battery is above 3.3V but cut out as cells discharge — the buck-boost keeps running until the battery is nearly dead. Switching frequency is around 1–2MHz. Requires an external inductor (\~2.2µH), input cap, and output cap on the PCB.

**microSD Card Holder** A physical spring-loaded connector that accepts a microSD card. Connects to the STM32 over SPI — four pins: MOSI, MISO, SCLK, CS. Your firmware uses the FatFS library to write a FAT32 filesystem, logging sensor data, GPS coordinates, and flight events to a CSV or binary file every second. This is your black box — if radio fails, the SD card has everything. The SanDisk Industrial card is specifically rated for \-40°C operation and write endurance much higher than consumer cards.

**SWD Debug Header** Serial Wire Debug — a 5-pin connector (VCC, GND, SWDIO, SWDCLK, NRST) that is the programming and debugging interface for the STM32. You connect an ST-Link programmer (costs \~$10) to this header to flash firmware. Without it you cannot put code on the chip. During development you also use it to step through code line by line with a debugger. This stays on the PCB permanently but you only use it on the bench and on the pad before launch.

**Raspberry Pi Zero 2W** A complete Linux computer on a board the size of a stick of gum. Runs Raspberry Pi OS (Debian-based Linux). Has a camera interface (CSI-2), WiFi, and GPIO pins. Its only job in your system is: receive a trigger signal from the STM32 → capture a photo with the Pi Camera → compress it → run SSDV encoder → send packets back to STM32 → save full-res version to SD card. It takes \~30–45 seconds to boot, so you power it on the pad well before launch. Draws \~150–300mA at 5V while running.

**Pi Camera Module 3** A camera module with a Sony IMX708 sensor (12MP), autofocus, and HDR capability. Connects to the Pi Zero via a CSI-2 ribbon cable — a high-speed differential serial interface that the Pi's Broadcom processor handles natively. libcamera (the Linux camera stack) drives it. You configure it to capture full-res JPEGs saved locally, plus a downscaled version for SSDV transmission. The wide angle lens (\~66° FoV) is good for capturing Earth's curvature at altitude.

**Runcam** A self-contained action camera — has its own battery, own storage, own lens. You just mount it to the outside of the payload box, press record before launch, and retrieve the footage after recovery. Zero electrical integration with your system. It's there purely for wide-angle cinematic video footage that looks good in your demo video, separate from the engineering data the Pi Camera is capturing.

---

### **Communication Protocols — Every Device**

**SPI — ICM-42688-P, RFM95W, BMP390, microSD**

These four share one SPI bus on the STM32. SPI is the right choice here for three distinct reasons depending on the device:

For the IMU (ICM-42688-P): you need speed. If you want IMU data at 1000Hz, each read is \~14 bytes (accel \+ gyro). At 1000Hz that's 14,000 bytes/second minimum. SPI at 8MHz handles this trivially. I2C at 400kHz fast mode would struggle.

For the LoRa module (RFM95W): the SX1276 inside only speaks SPI — it's not negotiable, it has no I2C interface. You write to its register map over SPI to configure spreading factor, bandwidth, transmit power, then hand it a packet buffer to transmit.

For the BMP390: it supports both SPI and I2C but SPI is preferred because you're already running a SPI bus and SPI is faster and simpler to share via chip selects.

For the SD card: SD protocol in SPI mode is the standard way to talk to SD cards from microcontrollers. It's the interface FatFS is written for. Fast enough for your logging rates.

Each SPI device gets its own dedicated CS (chip select) pin from the STM32. When you pull a device's CS pin low, it wakes up and listens. All others ignore the bus. So four SPI devices \= four GPIO pins used as CS.

**I2C — TMP117**

I2C for the temp sensor makes sense for one reason: it only has two wires (SDA \+ SCL) and the TMP117 only supports I2C anyway. Since you're reading temperature at maybe 1Hz, the slow speed (400kHz) is completely irrelevant. If you had six temperature sensors you'd still use I2C and just give each a different address — that's I2C's strength, multiple devices on two wires.

**UART — GPS (MAX-M10S), APRS (DRA818V)**

GPS is UART because u-blox designed it that way — it streams continuous ASCII NMEA sentences whether you ask or not. UART is the natural fit for streaming data. You configure one of the STM32's USART peripherals to 9600 or 115200 baud, enable DMA (Direct Memory Access) so sentences are received into a buffer without CPU involvement, and parse them in firmware.

APRS (DRA818V) is UART for configuration — you send AT commands to set frequency, squelch, volume. Then separately you feed audio-frequency AFSK tones into its analog audio input pin, generated by the STM32's DAC or a PWM output filtered to analog. So it's actually UART for control \+ DAC/PWM for the actual transmission data — a slightly unusual split.

**GPIO — Pi Zero trigger, status LEDs**

The STM32 pulls one GPIO pin HIGH to tell the Pi to take a photo. The Pi has a Python script watching that pin with a hardware interrupt — the moment it goes high, it captures. No protocol overhead, just a voltage level change. Simple and reliable.

The Pi also sends SSDV packets back to the STM32 — this goes over a UART link between the two boards, not GPIO. GPIO is just the trigger.

---

### **Board Architecture — How Many PCBs**

This is a real design decision with tradeoffs. Here are the three realistic options:

**Option A: Two PCBs (recommended)**

PCB 1 — Flight computer. Everything except the Pi and camera. STM32, all sensors, both radios, power management, SD card, GPS module. Roughly 80×60mm, 4-layer.

PCB 2 — This is just the Pi Zero itself (which is a pre-made board). You're not designing PCB 2, you're buying it. The Pi sits on standoffs inside the box connected to your flight computer via a short JST cable carrying 5V power and a 3-pin UART/GPIO connector.

This is the right split. Your custom engineering is concentrated on PCB 1\. The Pi is an off-the-shelf peripheral. Clean separation.

---

### **Final Recommendation: Two Boards, One Custom**

**Your custom 4-layer PCB (flight computer)** contains: STM32, crystal, GPS module, RFM95W, DRA818V, ICM-42688-P, BMP390, TMP117, TPS63070, LDO 5V, microSD holder, SWD header, all passives. Target size: 70×60mm. This is the board you design in KiCad, send to JLCPCB, hand-solder, and bring up.

**Pi Zero 2W (off the shelf)** sits separately on standoffs or in a foam pocket, connected to your PCB by three wires: 5V, GND, and a UART TX/RX pair for SSDV packet handoff plus one GPIO trigger wire. That's it.

The Runcam is completely standalone — no electrical connection to anything.

This architecture means your entire custom design effort is focused on one board where you have full control, and the Pi handles camera complexity without requiring you to design around a Broadcom processor.

[https://octopart.com/part/stmicroelectronics/STM32F411CEU6](https://octopart.com/part/stmicroelectronics/STM32F411CEU6)

BOM what we need to work on:  
Battery: Fly on 4S2P Energizer Ultimate, but test on rechargeable 2S Li-ion battery

- For 2S Li-ion battery use Keeppower 2S2P 18650 7000mAh 7.4V

For antennas:  
GPS (active, 1575 MHz, SMA male):

- https://www.amazon.com/MASWELL-Professional-Antenna-1575-42MHz-Semisphere/dp/B08CCX3K6F  
- Alt (quality puck, narrowband L1 filter): https://www.proxicast.com/shopping/ant-190-010.html

LoRa (915 MHz, SMA male whip):

- https://www.amazon.com/Antenna-Connector-Meshtastic-Wireless-Transceiver/dp/B0GQH2G9NZ  
- Alt (flexible): https://www.amazon.com/915MHz-Antenna-10dBi-Range-Meshtastic/dp/B0DY7KVJ1S

APRS (144 MHz HAB dipole, SMA):

- [https://shop.bigredbee.com/products/brb-vh-dipole-antenna](https://shop.bigredbee.com/products/brb-vh-dipole-antenna)

Camera:  
Links (ArduCAM 5MP Plus OV5642):  
    •    Arducam direct: [https://www.arducam.com/arducam-new-5mp-mini-spi-camera-module-for-arduino-uno-raspberry-pi-pico-and-other-mcu.html](https://www.arducam.com/arducam-new-5mp-mini-spi-camera-module-for-arduino-uno-raspberry-pi-pico-and-other-mcu.html)

    •    SparkFun: [https://www.sparkfun.com/arducam-5mp-plus-ov5642-mini-camera-module.html](https://www.sparkfun.com/arducam-5mp-plus-ov5642-mini-camera-module.html)

On/off switch:  
TE Connectivity ALCOSWITCH TMS6T1B1M1QE

# PCB Notes

![][image1]  
[STM32F411CEU6 STMicroelectronics | Microcontrollers | DigiKey](https://www.digikey.com/en/products/detail/stmicroelectronics/STM32F411CEU6/4935720)  
[ABM8W-12.0000MHZ-8-K1Z-T3 Abracon LLC | Crystals | DigiKey](https://www.digikey.com/en/products/detail/abracon-llc/ABM8W-12-0000MHZ-8-K1Z-T3/7172781)

# Parts Data

| *Module/IC* | *Operating Temperatures* |
| :---: | :---: |
| STM32F411CEUx | Ambient temperatures: \- 40 to \+85 °C / \- 40 to \+ 105 °C/ \- 40 to \+ 125 °CJunction temperature: – 40 to \+ 130 °C |
| TPS63070RNMR | Operating Junction Temperature Range: \-40 to \+125 °C |
| AP2112K-3.3TRG1 | Operating Junction Temperature Range: \+150 °C Storage Temperature Range: \-65 to \+150 °C Lead Temperature: \+260 °C (for soldering) Ambient Operation Temperature Range: \-40 to \+85 °C |
| SW\_SPDT | no datasheet  |
| ABM8W-12.0000MHZ-8-K1Z-T3 | Operating Temperature Range: \-40 to \+125 °C Storage Temperature: \-55 to \+125 °C |
| Conn\_01x05 | no datasheet |
| ArduCAM\_Mini\_2MP\_Camera\_Shield\_DS | \-10 to \+55 °C |
| NRST | no datasheet |
| BOOT0 | no datasheet |
| MAX-M10S-00B | Ambient AND Storage AND Operating Temperature: \-40 to \+85 °C |
| RFM95W-915S2 | Operational temperature range: \-20 to \+70 °C Absolute Maxes: \-55 to \+115 °C, Junction: \+125 °C |
| DRA818V | Operating Temperature Range: \-20 to \+70 °C Storage Temperature: \-40 to \+90 °C |
| ICM-42688-P | Operating Temperature Range: \-40 to \+85 °C Storage Temperature Range: \-40 to \+125 °C |
| MS5611-01BA03 | Operating Temperature: \-40 to \+85 °C Storage Temperature: \-40 to \+125 °C Maximum Soldering Temperature: 250 °C |
| MAX31865ATP\_T | Operating Temperature Range: \-40 to \+125 °C Junction Temperature: \+150 °C Storage Temperature Range: \-65 to \+150 °C Soldering Temperature (Reflow): \+260 °C Lead Temperature (soldering, 10s): \+300 °C |
| PT1000 Sensor | no datasheet |
| 1040310811 | no datasheet |
| micro\_SD\_detect | no datasheet |

# Layout strategy

![][image2]

### **Placement order**

Do them in this exact order — later components fill around the anchors set by earlier ones:

**1\. Antenna connectors first.** Place the LoRa uFL and APRS SMA (or uFL) on the top edge. Keep them ≥15mm apart — 915 MHz and 144 MHz are far apart in frequency but their harmonics can still desense each other. Snap to a nice grid so mechanical layout of your enclosure is easy.

**2\. The two radios, right next to their antennas.** RFM95W's RF pin should be within \~10mm of the uFL — the 50Ω trace between them is the single most sensitive trace on the board. DRA818V is a module with its own antenna pin; short and direct.

**3\. GPS module.** Place under whatever will be its antenna path. If you're using an external active antenna via uFL, put the uFL on an edge with clear space around it. Keep the MAX-M10S itself at least \~10mm from any switching regulator inductor.

**4\. STM32 in the center.** This is your hub. Center it so no SPI CS line has to snake across the board. The 25 MHz crystal goes within \~5mm of the STM32 crystal pins, with a ground pour under it and no digital traces routed underneath.

**5\. Sensor cluster around STM32.** IMU, MS5611, MAX31865 all share the SPI bus, so keep them within \~15mm of the STM32 to keep SPI traces short. MS5611 needs a small vent hole in your enclosure over it — mark that spot in your notes. The MS5611 also should not sit right on top of anything that generates heat (LDO, radios during TX).

**6\. microSD** on a board edge so you can push a card in without disassembly. SD is on the shared SPI bus with 10kΩ CS pullup you flagged as critical.

**7\. Switching regulators, grouped and isolated.** Both TPS63070s go in one corner of the bottom zone. Their high-di/dt loop (input cap → IC → inductor → output cap) must be as small as physically possible — this is the \#1 EMI rule for buck-boost. AP2112K downstream, close to whatever it feeds (probably STM32 VDD).

**8\. Battery input \+ protection chain** at the opposite side of the power zone from the switchers, so the input current path doesn't loop across the whole board.

**9\. Debug/SWD header** on an edge, doesn't matter which — you just need to reach it with a programmer.

### **Key layout rules for this board**

**RF trace impedance.** For the RFM95W to uFL trace, use a 50Ω coplanar waveguide with ground on both sides on layer 1 and full ground under it on layer 2\. JLCPCB's impedance calculator (or KiCad's built-in in `Board Setup → Impedance`) will give you the width for 1.6mm 4-layer stack — typically \~0.35mm trace with 0.2mm gap to side ground. Don't wing this; a 25Ω trace will kill your link budget by 6dB.

**Ground plane is continuous.** Layer 2 is a solid ground pour with no splits. When you route on layer 1 or 4, do NOT let a trace's return path have to jump over another trace on the plane layer. This is where 4-layer earns its keep.

**Star ground for switchers.** The TPS63070 GND pin, its input cap GND, and its output cap GND should meet at a single point that then vias down to layer 2 ground. Don't just spread them out and let ground flow through the plane.

**Stitching vias.** Along the perimeter of the board and around RF sections, drop GND vias every \~5mm connecting all four layers' grounds. This suppresses ground resonance and helps EMC.

**Barometer keepout.** No components within \~3mm of the MS5611 in x-y, and no ground pour directly around its pressure port. Airflow needs to reach the sensor's cavity.

Start with just steps 1-5 (antennas, radios, GPS, STM32, sensors). Show me that placement and I'll help you validate before you move on to power and routing.

# Embedded Coding

**Notes**  
*System Core Setup*

- SYS → Debug: Serial Wire  
  - SWD (serial wire debug): hardware protocol for processors using two pins  
  - enables SWD pins (PA13/PA14)  
  - SWDIO (serial wire debug input/output)  
    - bidirectional data pipeline  
    - uses C code to spit values back and forth   
  - SWCLK (serial wire clock)  
    - synchronization wire  
    - dictates when to read or write data on SWDIO  
  - Note: Might need an ST-Link Programmer for testing (what the debug is for)  
    - might be able to use Nucleo-Board as stand-in according to Gemini  
      ![][image3]  
- RCC (Reset and Clock Control) → HSE (High Speed Clock): Crystal/Ceramic Resonator  
  - manages clock signals for main CPU and every communication protocol  
  - HSE vs HSI: MCU already has internal clock (HSI; RC oscillator; 16 MHz) but is highly resistant to temperature changes; reason why we use HSE  
  - HSE: usually piece of quartz of certain microscopic thickness that vibrates when voltage is applied to it; usually 8 MHz; temperature resistant  
  - setting change dictates MCU PH0 and PH1 pins to use HSE over HSI  
- Clock Configuration: Basically sends frequency through a phase-locked loop that divides and multiplies the frequency to get ideal values/outputs

*Pin Configuration Notes*

- Alternate Function: outside of CPU’s manual control; so it can do better things usually  
  - usually CPU gives instructions to middlemen (i.e. SPI hardware) located in MCU and sensor (peripherals)   
- Chip Select  
  - CS pins set as GPIO output as the MCU is outputting a constant voltage  
  - CS pins use active-low logic (low voltage means activate) so set default output level to high  
  - set maximum output speed to high because SPI is very fast  
  - GPIO mode: Push-Pull instead of Open Drain (only one wire on the line)  
    - Push-Pull has two switches connecting to 3V3 source and ground alternating between the two for desired effect  
    - Open Drain has a resistor connected to 3V3 source and switch to ground but is slower because of resistor but can hold multiple open drain systems  
  - GPIO is no pull up/no pull down (no need to tether it to a set voltage)  
    - pull down connects wire to ground with internal spring/resistor  
    - pull up connects wire to rail with internal spring/resistor  
- IMU\_INT1 (Interrupt 1\)  
  - sends voltage when ready to send new data  
  - Pin Mode: GPIO\_EXTI15 (external interrupt pin 15\)  
    - forcibly pauses code (while loop) and runs IMU code   
  - went to NVIC (nested vector interrupt controller) and enabled EXTI interrupts  
- USART TX/RX  
  - GPIO Mode: Alternate Function Push/Pull (alternate to MCU C code dictating action)  
  - start logic: active-low; data logic: active-high; uses start bit (low voltage) to start communication  
  - set speed to low because UART is super slow and we shouldn’t overshoot because it causes electric ringing, signal crosstalk, radio noise  
  - use pull-up internal resistor to maintain 3V3  
- APRS\_AUDIO (PWM)  
  - uses pulse width modification (have ding explain later)  
    - something about RC low-pass filter  
    - Timer 2 divides the clock signal from 100 MHz to 1200-2200 Hz (human noise)  
  - also uses alternate function (not part of C main() code functions)  
  - routed channel 1 to PWM Generation CH1 and clock source to Internal Clock  
- SPI (CLK, MOSI, MISO pins)  
  - Mode: Full-Duplex Master (Bidirection data flow)  
  - Speed: High (SPI very fast)  
  - Alternate function Push Pull (MISO labeled this way too lazily but its overridden)  
- LoRa RESET/DIO0 (Digital I/O 0\)  
  - reset just resets the LoRa chip if it freezes due to temperature everything normal here  
  - for DIO use External Interrupt because it’s LoRa’s status pin on data transmission  
- APRS PTT/PD  
  - ask ding about this  
  - Low speed, Low output level   
- microSD\_detect  
  - checks if SD card is empty (3V3) or inserted (0V) because of insertion spring  
  - switch however is open piece of metal, making it float requiring pull up resistor  
- I2C (SDA/SCL)  
  - serial data and serial clock pins need open-drain because protocol overlaps wires  
  - active low passive high (SDA)

All things to test in code:

- GPS  
- IMU  
- Temperature Sensor  
- Barometer (?)  
- LoRa  
- APRS


# Livestream Proposal

*Proposal*  
Instead of taking pictures of the curvature of the Earth, by using solely the MCU paired with the camera module, with or without the use of the Raspberry Pi module to process imaging, we move towards livestreaming the entire process using a WiFi card and having the Raspberry Pi module run Open HD. 

*Abstract Explanation*  
Standard Wifi (TCP): Uses transmission control protocol to perfectly send 1s and 0s in order

- Process: Sends packet → Waits for acknowledgement → Sends another packet  
- If corruption occurs, process rewinds and resends packet  
- At high altitudes, constant corruption is expected making TCP unreliable (will cause long waits for acknowledgement and many resends, eventually causing video to freeze)

Open HD (UDP): Acts like a user datagram protocol using packet injection instead

- Process: Sends packets without waiting for acknowledgement  
- HD only changes the pixels in the frame that changed, so corruption would just assign the wrong pixels (causing blurry or laggy stream basically)  
- Not the standard because it jams other networks (no waiting for other networks) and no address (raw radio frequency)  
- Consequences: Frames are sometimes sent out of order (raspberry pi deletes them), messed up pixels (blurry stream), frames might be not sent (gone forever)

Raspberry Pi: 

- Would in theory exist on a separate board from Custom Kicad PCB  
- Gemini’s Recommendation: 40-pin GPIO Ribbon Cable  
  - very good for thermal control (boards far from each other)  
  - good for RF jamming avoidance (antennas distanced from each other)  
  - can help with inertia of entire balloon and stabilizing motion  
- Add 40-pin female GPIO header to custom board   
- Plugging in: Camera to CSI port; USB Open HD antenna to blue USB 3.0; custom board on top  
- Flow of Data: STM32 takes GPS and altitude data and sends it to `MCU_TO_PI_TX` through physical header connector, Pi takes live video from camera, overlays MCU data like HUD, compresses it, blasts it out of USB WiFi card using Open HD packet injection

*Engineering Explanation*  
Phase 1 (Ingestion):

- Light hits the HD camera, creating massive, raw stream of pixel data  
- Camera is plugged into CSI (camera serial interface) via ribbon cable  
  - wires camera directly to RSP’s GPU, bypassing CPU entirely  
  - allows RSP to ingest raw video at gigabits per second zero latency

Phase 2 (Compression):

- Done via RSP’s GPU crushing raw video using H.264/H.265 encoding  
  - looks at video frame, sees pixels that haven’t changed since last frame and deletes them from data stream  
  - RSP’s CPU takes UART telemetry data from STM32 (altitude, gps) and chemically burns numbers into video frames as on-screen display  
  - results in highly compressed data

Phase 3 (Transmission)

- using OpenHD we send the compressed data from the RSP to the WiFi card which transmits the data using radio waves using its antenna  
  - OpenHD is a custom Linux operating system that intentionally disables the WiFi card’s network regulators and does not provide the data with IP address  
  - OpenHD gives the data a “radiotap” header before sending off to WiFi card and hacks the WiFi card to blindly throw radiotap data into its RF amplifier  
  - without OpenHD the WiFi card would require sending the data in a regulated way (obeying federal network regulations, requiring IP addresses for sending data)

*Questions*  
How do we want the camera to be angled?  
How do we want to connect the Raspberry Pi? (HAT, Wire Harnessing, Ribbon Cable)  
Can we handle the expanded power usage? (estimated from 2 watts to 9-13 watts)

# Battery Proposal

*Proposal*  
Recommendation A: Molicel 18650s (Rechargeable)

- High drain cells with good internal chemistry  
- Maintenance down to \-20 degrees Celsius  
- Capacity: Pack of 2S2P of these provides \~5600 mAh  
  - will likely need more if using OpenHD  
  - highly viable due to not using RSP and Open HD

Recommendation B:  Energizer Ultimate Lithium AA (Aerospace Standard)

- Maintenance down to \-40 degrees Celsius  
- Very light (weight wise)  
- Will have to use 2S4P sled 

**DELETE TAB WHEN DECISION MADE**

# specifications for BOM

APRS:

**Resistors (all 0603):**

* **Material:** thick-film (standard, cheap — what JLCPCB stocks by default)  
* **Tolerance:** 1% (except R20 divider — see below)  
* **Power:** 0.1W (0603 default) is plenty; these carry µA–mA  
* R11, R12 (PTT/PD pull-ups) 10k · R18 1k · R19 10k · R20 (divider shunt — your tuning R, currently 470\) · R30 1k  
* **R19/R20 specifically:** use **1% or better** since they set the audio level/deviation ratio

**Caps (ceramic MLCC, 0603 except bulk):**

* **Dielectric:** **C0G/NP0** for the audio-path caps (C35 10nF, C36 100nF coupling, C47) — C0G is stable, low-distortion, matters for clean audio. **X7R** is acceptable if C0G isn't available in that value/size.  
* **Decoupling caps** (C37 100nF at VBAT): **X7R** fine  
* **Bulk cap** (C23 at VBAT, 10µF): **X7R or X5R, 0805**, voltage ≥10V (rail is 4.1V, want margin for the burst)  
* **Voltage rating:** ≥16V on the small ones (cheap, gives DC-bias margin)

**On JLCPCB, search/filter for:**

* Resistors: **0603, thick film, 1%, "Basic Part"** (Basic parts \= no extra assembly fee)  
* Audio caps: **0603, C0G/NP0, 50V** — but C0G in 100nF may only be "Extended" (costs more); if so, drop to **X7R Basic**  
* Decoupling/bulk: **0603 X7R 50V** and **0805 X7R 10µF 25V**, prefer **Basic Parts**

[image1]: <data:image/png;base64,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+T/99JNZP3HiRCDduHFDXbx4US+7ZQSM/+Ry8uRJs+weEz3QhP3796tr166ZdfQ2s3uNYUgA9BYLA/nusUn8oMAFocARkh0UOE9Q4KKRTuB27typf+SfeeYZ3YZIfvCfe+45M9o6ljH4ZevWrfW6iJZsh2zh8fnnn9ePkyZN0snOk/L33XefPh4SxokCzZo109tWrVql11966SW9vnDhQr2O8Zuwbgufzblz5/T2p556yhybxA8KXBAKHCHZQYHzBAUuGukEbsyYMeZHfv78+XpZIl8PP/xw4AYgArds2TK9LgK3du1a9eSTT5q8V199Vb3xxht6ChzJk8eWLVv+fDALOQ4EToRSBE5EEAkCt2HDBj38AKJuCxYs0MsicIjSkfhCgQtCgSMkOyhwnqDARSOdwAHMJyiS9Oijj5r8MIH78MMP1RNPPKHWrVunXnnllcB2LCMKhlHUha1btwYEThLKgTfffFMP6ikCh2pagO0QOKmCFYEDmE5HooWYSkcEzk4kflDgglDgCMkOCpwnKHDRSCdw06dPVz169NDL69ev12IkVZdhAjdixAgjSahWdW8QtkCJZN28edNscyNwyHvkkUf04wMPPGDyReDscnYVKtZxPoAROAIocEEocIRkBwXOExS4aKQTuLffflv/yKOTAjohYBkTSYMwgRs5cqSRNETa8IgIGmQNyDZEzvDYoEEDs38ygbOTkErg9uzZY8rjXClwBFDgglDgCMkOCpwnKHDRSCdwAG3JRIikahNIZEzApM+ovrz//vtNvjxK+zl0WsAE1g8++GCCmOHRFTgB26QTA3j66aeNSMr2q1evqr59++plSJs8x4ULFwLPZZ8ziQ8UuCAUOEKygwLnCQpcNDIROEJKAhS4IBQ4QrKDAucJClw0KHAkLrgCJ/OjVqxYUa8PHDhQr99777163EEwePBgU76kQYEjJDsocJ6gwEWDAkfigi1whw8fVtu2bdNjFlauXFnn4buAjje3bt1S1atX10PRUOAIIS4UOE9Q4KIRReAkKgHsoUByhTv7goA8tGkTcPOVnqvpwA3a3jfXcFaHwiEsAle3bl01efJkvY52mi4UOEKICwXOExS4aGQqcA899JD5sf/mm29MRwXpXZoLcLxhw4a52Tq/T58+ZhmzQuARgwynA4MKZ/L6siUON8BGjRrp8fS++OIL/XkBVapU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>

[image2]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnAAAAIVCAYAAABRKsJKAACAAElEQVR4Xuy9h7sVRdqvPX/C951zvnOdN405J0ARFSQjKihgRDKI5IwgIGYwI4qKAgKCAQUzBgRRzDmncZxxgjNOdmac+KbTH3cxT1urOuy1e62191qbX13Xfa3qqlrV3dXVVb9+qrrrO3vu+d1ICCGEEEI0Dt/ZY489ol189x+/5rdt3x9up8VVki4vrtrp8uIqSZcXV0m6vLhK0uXFVZIuL67a6fLiKkmXF1dJury4StLlxVU7XV5cJeny4ipJlxdXSbq8uErS5cVVO11eXCXp8uIqSZcXV0m6vLhqp8uLqyRdXlwl6fLiKkmXF1dJury4aqfLi6skXV7cLv93LPK7caKd/u96fi88LSwrbVZ4Gllps8LTyEqbFZ5GVtpywtPCstJmhaeRlTYrPI2stFnhaWSlzQpPIyttOeFpZKXNCk8jK21WeBpZabPC08hKW054WlhW2qzwNLLSZoWnkZU2KzyNrLTlhKeFZaXNCk8jK21WeBpZabPC08hKW054WlhW2qzwNLLSZoWnkZU2KzyNrLRZ4WlkpS0nPI2stFnhaWSlzQpPIyttVngaWWnLCU8Ly0qbFZ5GVtqs8DSy0maFp5GVtpzwtLCstN/ZZYrb4x8mOX7N74eH/rztcvMI01Urj6y4PH+95RHmVa08srZDf3PzSDu+auSR5a9FHllxef62mEeYV9526G9uHmnHV4080vx52+XmEaarVh5ZcXn+essjzKtaeWRth/7m5pF2fNXII8tfizyy4vL8bTGPMK9q5ZG2nZZ3NfJI8+dt78rjHwJul5rz/bbt+8PttLhK0uXFVTtdXlwl6fLiKkmXF1dJury4StLlxVU7XV5cJeny4ipJlxdXSbq8uGqny4urJF1eXCXp8uIqSZcXV0m6vLhqp8uLqyRdXlwl6fLiKkmXF1ftdHlxlaTLi6skXV5cJeny4ipJlxdX7XR5cZWky4szfzyEusce/xYn3OW3bd8fbqfFVZIuL67a6fLiKkmXF1dJury4StLlxVWSLi+u2uny4ipJlxdXSbq8uErS5cVVO11eXCXp8uIqSZcXV0m6vLhK0uXFVTtdXlwl6fLiKkmXF1dJury4aqfLi6skXV5cJeny4ipJlxdXSbq8uGqny4urJF1e3C7/d2xsld9y/WlkpS3HnxbWXH8aWWmz/Glkpc3yp5GVthx/Wlhz/Wlkpc3yp5GVNsufRlbacvxpYc31p5GVNsufRlbaLH8aWWmz/GlkpS3Hn0ZW2ix/Gllps/xpZKXN8qeRlbYcf1pYc/1pZKXN8qeRlTbLn0ZW2nL8aWHN9aeRlTbLn0ZW2ix/Gllpy/GnhTXXn0ZW2ix/Gllps/xpZKXN8qeRlbYcf1pYc/1pZKXN8qeRlTbLn0aY9jvJMdys8dhy4ypJlxdX7XR5cZWky4urJF1eXCXp8uIqSZcXV+10eXGVpMuLqyRdXlwl6fLiqp0uL66SdHlxlaTLi6skXV5cJeny4qqdLi+uknR5cZWky4urJF1eXLXT5cVVki4vrpJ0eXGVpMuLqyRdXly10+XFVZIuL26X3xtCFUIIIYQQjYDmwKXGVZIuL66SdHlxlaTLi6skXV5ctdPlxVWSLi+uknR5cZWky4urdrq8uErS5cVVki4vrpJ0eXGVpMuLq3a6vLhK0uXFVZIuL66SdHlx1U6XF1dJury4StLlxVWSLi+uknR5cdVOlxdXSbq8uF1+WeCEEEIIIRoMCTghhBBCiAZDAk4IIYQQosGoWwHXpUvn6MQTT8jliCMOcyxdekP00EObEnm0Bj/96Y+jf//3vyXCq8n9998Xvfvu29Frr70SnX76aYn4RuX000+PfvGLnyXCW4N99tnbXcdaX0shhBCiCHUr4FauXBH94Q9fx53o11//1vG3v/0lDjv//FnRX//6Z+dfvHhRIo+WZtu2p2ve6R99dMfoj3/8fdS+fbu4XA466MBEukak1mVXLu3atYt+9KMv3LFQ1mG8EEII0drUrYCDzp2Pc53o+++/G4ftt9++0euvvxoLuLvuWu/8F1xwQeL/Lc3QoUNrLkKuvfZqZ3nDz35+/etfRocccnAiXSNS67IrF6xv1CeO5dlntyfihRBCiNamrgUchALOuPnmm6KRI0dE7dofkSrgTj75xOjNN1+Lnnrqiejiixcm/o/YuuOOVS7vGTOmRQcffFAcZ3liBYRXXnnJhZMP4cOHD4see+yR6MMP34/Gjh1Tki/xP//5l/E2H9sj7ccffxg9//yOqFu3biXpu3fv5v5zwgl9orvvviv64IP34v357L//ftG0aVNikbNp08aoU6dOcbydz333bUicz5Qpk9zQ5C9/+VU0d+6ckvIcPXqky8tYtWqlE4S2zfA06fbee6/EPvzju+WWZSXia/Dgs5348cO2b98Wb7/66svRxIkTSvJYvvwWF3/ooYe4/Ehz5ZWLo2OO+fY8gfNeuHBB9PLLL0XXX39dSRyceeYZ0TvvvLXz+r/uBL4fR3lzTSlvrJm33768JI5r/MEH77phao7l4YcfSOQvhBBCtDYNJ+B8YQJpAg5xQdigQQOcKPj73/8aHX98l5L/ffPNH6KLLrowOuOMM1zal156wYWPGjUiFkmLFl3ufhnKPeecwXE4w2qffPJRPJyLVcw/Xl/APbXlyZ0i4MHouOOOjUaMGO4sZsz1Im7ZshvjPBEVHJNtDxx4asnxDhkyxJ2HxePv06d34nyefPLxkvM56qgj4/8w3Gp++1/Pnj2iF1983oV99tknTpBigZo7d6477zFjRrt099xzd2IfBxywv4tDZIX52raFIcZs20TrFVdcVnKOJuB+8pMfuSHMr776mdv+8Y9/WJLut7/99U4x+Iw7FkTpmDGj4rjLL7/UlU3//idHJ53U14k8E/B+eRPHPMKnt26J//uXv/wp+tWvfrGz/kyP1q9f59JJwAkhhKhHGkLAwaOPPhxt3fq0ExV+fJqAY3vevHnxdo8e3V0YliheeqCDR7hYvA2Xdex4VEmeJj7CY/HDTBT5aUzAdejQ3okPf44a8f68qvHjz3Nh/vHs2PFc9N5775Tsx8Dq57+wUc75fPTRB6nH7nPfffe6+YSdOh3ttrHMIZRsH+ExhvsI8zcB9OMffxGHWZobb9xl1QsxAZcW1q1bV7eN0EUQWzyWQeLZH9tYOX/zm1/F8XbtTzttoNu28kaAM9fN0r3xxqvRunVrS/ZNOgk4IYQQ9UjDCLirrroyuummpc4y48eHAo7hN7YZXrU0e+21pwtbs+YOJ0zw+6KKzp2wCRPGleTJMG3asfhhzEfzw/D7FjifXr16JvIwQYHQsDBEA0Op4f8hFHDlnI8JOERKmJ/BkCJpliy53m0/8cRjTiyVu4/wvEzAffHFt9azME1ImoDD8kjYzJkz3DZDo6Tz0xDPEHWYH9ZHriHx5567a6jbyrtfv5PidMyr/POfv4nOnzM7ka8EnBBCiHqkIQScP4TK3Co/PhRwzE9jm+HTMB+sM+FcLcCKRNh1112TmqefR/hfLFd+GP5QwN1555rohz/8QSz2/PSVCrhyzscEXFNihDR8BgXLFILGjol9sJ23j/C8sgRcWDY+aQKOoWfC5s+f54bPsRIi5v00xDP8iTWObUQ8FkyGhLc8/ZSLDwWcX94DBpziwmxo28+3qTITQgghWoOGE3AhodhiXhbbaZ0887tsLpsv8E45pb8La9fu8NQ8/TxCgWGfm/DTmEhhHhbb/ry9MI80QdEcAVfO+ZQr4Ngn6djHeeed2+Q+GLq1fYTnZW8HVyrgZs2a6cKYl8c2L3gw/81PQ7xZC5nD5udhQ6h5Ao6XGQgLh3bLKTMhhBCiNWgIAZc2PGakiS22v/e9T+JthgeZi3bYYYc4MfW73/3GvfFp8UyG9zv9tDwt31BgsG1zxWzbRMoNNyxx2wceeEBmHmmCojkCrpzzKVfAMUxJOub1mWCyfRAe7oM3fG07PC/b9j/M65dNGmkCjiFTPwyLH9ZAPvTMNm/nYn3Diubv19LzpivbeQIOvv/5Z+68u3Y93m3b3DpeQvHTCSGEEPVA3Qo4xIF1xmGn7OPH83YkYQy7bd78qHuLEZjPFf6P4dQ//emPzkKEMOGFA8KnT5+auU8/nDdQEU7M0bL4z3/w/Tge6xQi4N5773bbCBksPHxagzj2w5ud4b78N0XZh+XNOfkfMfbj/PNhCNQ/HxuCNMLPmIQgoo48skMifMiQcxL78ON5WxUhyz4QXTaEivjmJQi/bOxcQ9g314sXVexcl992a4kAhtWr73DXjbdVX3rpxZI43p5F4JEHL77wSRR7uzcs740b7y/571tvveGuDVa82bNnuuFaLH718JFoIYQQwqduBVw16N27l5ugHoYbDLfyYkEYnoV1/Icffqiz6tmcq6bg7U2+B2fbTK4P01QDzsffTxH4Zl0Y5pO3D4SWnRvfbrM3WpsLL51gITMRmgXCMAwDrHL77rtPvI0F0d5ibQr2aefHdQ7jhRBCiHqgTQu4apNnPWoLTJ062VmgwnAhhBBC1BcScGVi30IDPgIbxjcytuYsQ6NM6A/jhRBCCFFfSMCVyVlnnenefgR/+aW2wCOPPOTmnvkrOwghhBCifpGAE0IIIYRoMCTghBBCCCEaDAk4IYQQQogGQwJOCCGEEKLBkIATQgghhGgwJOCEEEIIIRoMCTghhBBCiAZDAk4IIYQQosGQgBNCCCGEaDAk4IQQQgghGgwJOCGEEEKIBkMCTgghhGgh9txzj0SYEEWoWwF3/fXXRffff7/jwQcfjP2wZs0al2bWrJkRLvxvuey3377RTTfdVFEeeRx77DHRV1/9PBEOxxzTyZ1XrfbdWrz44outfk4vvPBCtHXr1kR4cxk6dEj0l7/8pdD5jBkzOvrmm28K/TePM888I/qv//qv6D//8z8TcfWIld/rr7+eiKsl1IFql321wFVaP/fdd5/o4osvcnmdeGLfRHxLcNdd66Nbb70l2nvvvUrCu3fvXtJet2/fPvFfn9/97nfRoEEDE+GVQp6ffPJJbj0499wx0WWXXZoIrxUjR46Ifv7zXX0C+/XL6bbbbotmz54ddevWNU7vx4dQ/mH+9U5enyiaT90KuPfffz/685//HA0bNjS65ppr3E04ePDZ0YIFC6Kvv/7apZk7d07uzdkU+++/X7R8+fKK8sjj9NNPy8ybivzKK69kxjcq5sLwluRnP/tZ9IMf/CAR3lyeeOKJwudjAqLIf/Ogga9FvpVy6qmnJsLAHJ1TGFdL6qWMDj74oGj06FElYbhK6yftx69+9SuXV58+vRPxlZJ1PX2OPLKD2/+GDRtKwvfaa8+oR4/uO0X7a1Hnzscl/heCmzPn/ER4pfCwYy6MM6699lr3IB2G14pf/vKX0axZs5yfujF//nz3cDNgwIDo6quvjl599dXo3//93+P0iLmPPvrInQNpYP78eTtF0FfRX//610T+HTrki+XWJuwT6YO7d+9Wkoa6wwNK+F+RpK4F3KhRI53/nHMGl1x0ntjMn3dzlstrr72aCKsWVNgwzLAGMAxvZO6++65WPycauX79Tk6EF6ESK5q5MLwS9tln7+i6667b2dhflYhrLbCwUE5hOFj50VGGcbXkySefrHrZFwEL1e9+99uSsNWrV1elfh5yyMHuHKst4BBgjz/+eCI8DXO33HJzIu7yyy9PhKXBQ3q1hhX79i21Rk6ZMiW3HnA/IaTC8Frxve99z5WvbbP/Rx55pCTNAw9sKhE1y5YtS5zD8cd3cQaOMP/p06clwuoNv0+85JKLEw8APJh06nR04n8iSd0KOJ9QwPn88Y9/dEOqDz/8cPSb3/ymJB1Pf1988UW0ePEilyZsSI0tW7a43wkTxrv/b978WBzHDccT05lnnhm9/fbb0QEH7B+NHXtu9Le//c0NY9nwwdatT0fvvfdeSb4//elPE8eN+fiee+6Oxo07L/rwww9L4nlSt/0xFMn+CKeDfOaZZ9zQBFbIVatWunBufoZRTj75pDiPww471OX5hz/8wd34y5ff6rZfeOF5F3/CCX1cmU2cOMFZiRAD9l9zb731VvQf//Efzs8veVqaO++805UnN94777yTaPwY+sZR1j/+8Y+j//7v/45++9vfRl27Hh/t2PFcvI9TTunv0v/whz90DZHf8JhlAcdTph3L7bffHqdhvxznpEkTXdxzzz3nygNLjzlLyxMtHelDDz3kwp9+eosLP/DAA9w14OmfMuXc/HOBl156yf3nT3/6kxv6wL355pvuybGpPHgIwXHN7Rz8OnLHHavcMAjWDsrJhnK4PjiuDed01FFHuu3NmzeXlI3lwxM6ZbF+/ToXTlkQPn78uDjtSSed6H7btTsicY50BuY4h7Tyzqo35Gfu//7f/xs9+mhpZ5RXfghtc5wj6bneWKcGDhwQ50EnHF5Di6MecJ9wz+Deffcdd0+uWLEi3ldWXaa9COsyeZnjHrL/3nfffW66Bfc8jnIgj549e7hrgkWE9sPvfNauXRvnRdlgWUirnzNnznACaN68C9z+aBv8MsRCQ5lQP7788kuXF+EHHXSgy4drRrmF+QKjFbRv27dvd3Vs8uTJcRznsnLlyujzzz9392uvXj2jTz/dNeSIYz+cj59fCHWNOsDxcZ38OMrV/NRN2lXuky+++KEbfSD80ksvcfuyYeDVq++Inn9+hxN1nDfX3dpbHui5trTrYXvLfWLtC22dtXcm4D799NO47lx55WIXRx3Dbdx4v9u29hrHMVBu1AHfGsQIENfn7LPPcudMvUFw0Q7RlmH1o67g/LKAjh2Pcha3MDwUcFdccUXJkPLSpTeU5Dd58qREHljmGXnAce6MWj311FNum/Ok/HD0JT/5yU/ivuT73/9+nIc56gRlRb2wsoLw2tD2WRlSzxiuppxx7J92glEM2g0rQ79P5CHGnF0v9oHDUmlhwLXl+o8dO9alIczOD2fnRx1mH1guechtlKkmRWl4AYcbMWKE8x9xxOElF4wKSOOInyc8E0QhJuBoYGikLZwKRgNt2wiNm29e5vwIQv+YEJBUbD9fxJ6fhg6CTtu2aej9eISO7Y9G0Z6waDT8pxYaYp6+OTccDYfFmYCzY+FmpBFizh3bdDTcaP7xmUnfnDWUdDg4GgfibSjb9oXzO3kwAXf00R3dNiZ9HEKa8w8tWgjTpUuXluThCw86djtOGijiaaxpHOycuH64TZs2xmKH4RvLzxfuDFHQMHB+dD40qhaHM6uvYQLEBBvXBMdTY1N5mIDDKsBTtzniDj/8MDeXjevINv6///3vbl4mDQ+ONP45cAx+2RBOWdB4hWWB38oNh8h84IEHXKfvnx9wb5jjfPkfAsjKG/LqDR0pLswXrPwsvn//fs5vT91+HNDY0zn6eSBawmvIL+f+8ssvx+WEUMbxsGUCzury73//e7ft12VrN+w4qMt9+56w64B2OoQC+zCHaOX+4ppj4eN/iCccUzHY5v6mw/Xz9Y89rJ/Dhw8vabNwJm78sIULFzr/WWed5bbxm4CjjMnXztn+R71E2FP3uPdoB2xflDEPd/jPOON09z8TRJRDuRY4e1hgSBfx6beBvoDjIYOyxI/FkLQWhzPLGW3bkCHnOH+XLp3j46W99Ydj09pbQDD42ybgbN/cBx988EEcz4MNggm/f79YPI75rPiPO+7YkjjaahNkPHjzwIyf8kak+scB1EvKOgw3Acf9zXXhXvPnFYYC7o033kjkAZSr/yDMfc29g3hnrt8xxxzj+hI/r8suuyxug3iIwNGOWryVFe14eG3s4Z0yROhbGZuIszxwiHD8YZ/IaFpogcP5FjiGjPmfH8/8ZDs/nJ0fD9B++SCm/bzbGg0v4GiU/G2euM2PQ7nTyPOkRAeEtSHMA4uMb3Uz6BB44qBBJV/+z7bFc2NYpfUtWT6+uMLy5seFQ6g42x+WBOswDSoolRmX1niF8BQTlhuOJ31/m4nmnEfYAVg8w6KnnnqK8+M+++yzGI7TT28Czg9DmFgYNyaNMpN52Q4bXLCOyc/HHGUWCkkTiRbGefj5mkMs8vTKjc+TJc4/FwitcCZAbPu00wbF+TWVhwk4+y8WSX8baARp3MzRoRNOHnR6lsYEWlg2lIX/BG9lYU/wuLShlhBzNhzH/nCUt8Vn1ZtyBJw/hGqOToDrgePhi7iLLtolVHx8y61dQ8KZM4QL00NYl80q4tfl8LpZXQ6vmzlrOxAFP/rRjxL7RAjgaGv8/4aWfxz1k7KjDBHgYV4+ZvVDUGOFNCFjdSHrmj377LNu286Phyh+EQfUCdqmMI7/FRFwwEMm52LDfybgeBAM5wEycuDXURNwNvSNVcy3woKJBERKVnsbtifhECoP4DgbskX4YgW0eMrEHugBd+ONNzo/Qs7PCzFv9wTnikPkYKGzfsEHsZQWzv3bvn07J0bMEu1jAo6HYvoAv4/zCQUchH2OYX3Jr3/967gvsbLyh6FxVlZZ1yYsQ9ouv83B+VM+/D6xKQFnowM8sDPVCSMM/SL3AfGcn68BaJ/ZN2loSw499JCSvNsabVrA+Q1pHtywPDlfddWVcZhZssK0PsOGDXMWEfxmcQrxK6v/5Ae+gGN/9kScBlYHJtsydIMrR8DhEA1hGE9h/jbDFFh+wk7P4un0LrhgrvN//PHHif34pAm4X/ziFyVhmOftLbzQ2gKhSAFzlBkNnh8H1snhDwUcJncaCnO8wcUTfJhHGqGAwzSPw2LWVB6hEAgFHGILqyvHaxYiE3AMOeMYHqfs7T9h2VAW4cMHZYGlBj/OnzOahbksMYDLqjdFBBzlhzBFTNBxMV+K8giH5IGhmvAaEp5WD4ywLvsCzupy+B8jvG7msgQcApyhKF6qwoJfroBD6JgLj8Fn15DVa3Fae4mrKQHHfceQU5if1WF/ONmnqIADLCFcT6x/JuAQNGaBMbCM2r2PM9FAfaB8EUI4f0geR3vLg3NWe1tLAUd5E08dZUjwhhuWxA8egIC1OokVyj8OQAAyFSEMtwcwHnQxNvhz5CC0wNF+hXlAuQKOYV7rSzimcgVceG1MjIZlWE0Bd95557ltP94nFHDAfUq7isOiH/6nLdGmBRwufEU87e0Wu2EZWqNy2k3AnCPm6fhpMRf728wTCa02Pn5l5UbkXGw7tMBxA4f7A8b6/W2c3XTctGGnx/wGnuyXLLnebZMWgWD/fe+9d0vysnRhp2fxdHrcrDbPxm5onuJsPoKRJuBwdIphWJjOCEWKn54yszeZ7PMEZlGh8WPbF3BYeexpkePG0oCzPLAE2D5o2HhK9o8lFHD2kgb7aCqPUAj4Ag4LpB9nzgScH+aL5rBsOAauS1gWfh6+gKP+pb0Rai5LDOCy6g3DdLgwT8gScJSfbVN/zYX/t/ThNWTIzob+6EgtLaKBMgnrsi/grC4jUtLqcnjdzKUJOMoEZ3PLGFINBVz4ggfO6qeVjwkShunCaQlYIM1P24TD35SAw0qFRcx/o9SG/JhjyzCfCRAbNsfPUJr/mRPqTDi/1/CPzbApAP78KSwnVtawbt262I8z0YDV0MI5Jtpj277wwgtdexuKQZ9aCjge8sNrYyDSedCw7bTpOtQ7G2b1MQGHcEOo83Djx4cCLgvqwYwZ00vCQgFHX+LPe0MglSPgmN8ZXhtr98IybK6AC4c5cTbiAMxV9efuUo5W50MBh3CmDbRts9TZMHFboyEEXN4TM0LFf2JhjoP56dC5GWiQuKh0OKHYAXuKpEGkobJ5FzTYVB5EIJNsmazKZFz/vyh8/7XvEPvmD1DRyY+bhpvEPjWBtYV45j7Y/phrwP4IZ04CE06puMyjwPGky1MszhetpLEOiCcmygbhYA2wby2hUjNka09SmMYtzsBZo0jnQYPK0DJWNG5S3zoEJuB4uufGpzPFEkCHG+abNbRHWnN+epw9wTL53+ahbd36tDsme2rjPGyeFAKOesDxYOHhemEJ5djozOkQuOl5qqWj9V8IAetgEf5Ym3CEsZ2XB8dFJ4+zvGjkbduuIxYbOn9zdFKW3q7V+eefH4fZPBycXzaUBdfaysKPoxO3BoyywLF//zzNmeXOXnqw8s6rNwgoHEIhHCKy8uM+pfyoL1Z+4f79eYthXHgNLY4GHIfVh0nlzEXiHg/rMuWC8+syLqzLadfNnD1c0XHxhM852KeAuP50UrQ3TKK2ztzmTPrfQsNZ/aR8ebiiDUFUkd7mFvrpud/x2zwl/HafZF0ze0uV+WaUB2Vjw6SkwVFfH3vsUXePUg7EUTc4DixnlKXVGb9TNSjz0GIEiGF/LiDlsmjRrrlmtH1+u4izssWZWKfN9sUM7TPX3p8fFsKwur0gBdw7ONu2OYs2p4r2wR954bh8qyLOHniw/CHYsVwZdl/hTKRQZgxNhscGTAHwt9m/L/jZpux5CLcw5q7655AFc+h4KOE+8//r35P0JfRv1pfwQoPVLSsrX/BbWdkLfv61sWMMyxBBhjHCz8Of5+xfe9oRHsjsocPS+28wM/xO+8N3X3v37uXmuNkLM5yf/zkV6riVMe0z1ww/181Gy9oSdS/gaHjtDRMaON/sb0/VdMhUWmto6CCooDSidKrMneCmCC1ZqHrEDx2wvZnIXASeTG0uAjezNcJMVPafIoEO3F5sCCFvnOUNjONzA9GQ2nCozfvhhrD9ccw2MZobBMebfJwDIpUOhDkkNM7+edlbmTjizNxtw2w8cdOJYdWho/IFrTmbRM1TuDlL45vQ0z4TgICzN0cZ6qHBTfvMAU9eWR+itPLGYZEySxfOblY6DTpfOu5t27a5t48It44bx3nQqNFBUodoVLm57WmOa0kDhqOjYEg8PBbqEv9jvxzXvffeW1JmWXn4b7TReNHZm8NCx/Fb/eU6MJcm/GgwZU052QsUaWVDGFYByoLO1C8L3vgzZ2KZfeP8lzW4PuZoeM2qh+OXzjWv3mC9olHF0YBaOFj5cWwcA4132kMULu3tOkDkhtfQ4qwe4LAGmZXMnNVl39l/mWMY1uXwuvlvyrF/0nGdcVwbxIIN1zAsxQsVOMLIj7dcEe60Mdzvfv204yAPe9vVt1oZiD3+z/Fy/tOmTXXXzOoC14y6EF4z/os44z4kjLaFBwDLl7LhnDhWHgBMVFOnbd4qv9QZ9h8KJ3sLkGvuvxBiefgvMfCQQjvNPCr+Z59R4eUMHPWC4TwEBsfKL+cb3pO0t/52CA9xXFOsPORtUxPsRQSEsh0z1nLqMuWItdLaa3vL1wQxZcA9hrCh/H1H+0Y7jFilnLk/EBu+oPEhL7Nw0VZafjbfFZibxnEibnjb0xxtT5hfCH0Ex4SwoX2xvsYe2OhLuBetL6Et5Przco+VlQksXoDDUVb0h+G1IU1Yhvfcc088nYWhenvTnP9hfQz7RK4v9y/lYvPVqCPkwUOFnRcWTct36tSpLszOD2fnR5vJtefBjrbHHkpoF9M+ddPo1L2Aq3fCV8BF09BocEOF4aK20KkyHOa/jFAP0KHZm4BCZIFIbc32ltGR0MKMyPItdk3B/7MszaI28GBCm5dmKW50JOAKwFtgmGN56SBtUqpIh6ci5l/wdFnOl95FdeGzB8zrDMNbCyyLWLywmKRZ5YQAa2+x8rRme4u1JxxGZgg6fMO2KfzhQlF7sKQy/BqGtwUk4AqABYmhSv/7X6JpGN5i7lHYCIrdF4YVJd5EHtbehuGtARY0Jv3zAk3WW7xCtBQScEIIIYQQDYYEnBBCCCFEgyEBJ4QQQgjRYEjACSGEEEI0GBJwokVhrTp/kXYhdgdYNooP0Ybhoj5RGyUaAQm4DMIv8rclbDHs1oAPP/LtpDBclMfQoUMSYSJJvd2/qvONBderNb4bRtvc0nW3JffHx6DzlkKrFRgOwrC2QJsWcHzhudxFmQ0+ccHXm/1130KK5FsvcG64MLwl8dcaFN/Ch3ZZqSPt8wSsNMKKBK197cqBL57zdfow3GDtwnCN4mpRj/cvxxSuoQx8mZ4v0rOW7Jo1a0pWx2AFGYMvzvOl//D7YXwdn6/Ws1IDaVpDcFid9de3rBV+mdgal7YdpjUQRJaG5RD9PGxVCgSMH86KALRRto+WwtrmvLpbTcq5V6qNOdtmNZO8tsJgua+i7cYnn3zSEO1mEdq0gGNJIH8NTn9plzzCxXlDwnwbCc6t3MocLj1WTfIa3d0Zfw3ENBrhK+58t8uWnGOVBdY69OP5on41v/3GsnO2Di7U0/2LgPDXhQTWu2UpozBtuAYqy5P98pe/jLe59uRF2bGSCUsL+ulZ8zjMsyWgzvbv3y8RXgsQvayHadusGY1jneswLfWQcvbbO1sD1hao91mwYEHJdtZSf9UivDesbc6ru5XS3Hul2rBkm389+KZeuDxlGnx/L2w3+KZomC4Nltfy99mWqGsBh9Vh3bp10R13rIobN9aJY71A4EvuXFC7kD16dHfh8+fPd9t8fZn1CfGPHTvWfcmbNepYI8/2wf/ZB+vSWZhVatZSvfPOO93i8/4CwX6+LPRrX3lm8WeOj3Uj/fOgs2C9UxoE/Cw+HXZgLNLN0wXLfrCmnIXzdMKTYNoTPOlWr17t1kbk/FiMm31zjv7iwZyffQgzFHCsY8ki06xhZ+sTcq62KD152WLHwMeLsRaECwNbmXAstm+eeDlGGte+fU8oSc9qDKFFweB6cszsm0YubFivueYaVybhsjY+XIfwOO1acZxcK+LDa+XDdWINxHB9StahZJ1Yyo7FszHPh40Qay+Sv784vQ/lyAMF1qqRI0e4MPIIh0gJw0LAdaZc/HV1gUXS165dGy/unAf7YX1d1vMMy546sGLFinh9VeBYEBH4Ke+wk0w7B6B87Iv5rLto9cjKmk7YnqTtXrb1IW3brjlWJfZNPUq7B7hvcJQ19Y2wtPvX/49///plzuognCNrOPrp8+5fzp81KimDsA4Ai7qH68Oy1mbah2nDtUaxrPkCjuuGo24hTMJVNWyheh/Kh/K2dsXWkQS75uFxc11Zc5T9sTa0hVPXWDOT+mbnb+VHPaHc7fpZved31/Xddf+n5QF2TyEuWHA+PA8DS2T37t3iba7FCy8879bz9NcMhhtuWOL247d39BG4sC7D3LlzSrZZwD28T3yo49RN5jf64Va/qcNp9c/+G94bvoCz//r9DjR1PwD1iHKh3O2egHLvlXCfCHRrG6yu8GttAwvcH3LIwYnjMLB0cj7cP1iMWavV4rjm/uoa5Mu+WF+cdVJnzpzh1tm1eky5Wv/06aeflPRPnAP9RrjmqQRcK8DNw6LRtj1jxvTYj7OFb20xYP9/fjrftI+51vw0TCxCjvBhmwnGDz/8sPNTqVk2xSay0qCwnZYvogBHw8A2lQlniycDN4b5Gb5hgWnbNrgZyZPKa43do49+u+4fy3bZ0zc3hH/OLJFkotUaATp8i8fxxXs/3uIQtdbwbd36dPT+++87vy1wbukQKZSfDTtw3l988UP3a/vAITi52ei0uOGIo9EhreUFCDgaWD8MWOzb3+aJ0fysl+kv+swC4NYRG3actu0fp10r/7xw/rUCGhHKwg/DsTg8fhoInAnIDz/80C3UjZ/hz88//7wkLxZw9vOiPljjBwx/Wdovv/wyDudp9Z133okXGQd/DVk6La49fqwNNHr+fny4Jlgr8LOgNfWZuoTQ9Tt+GmkTNBwLzhYqp5NhgWv8WecAlI8JsieeeMItdu0fC87WPkVg4ywOQUQnjZ/671tquQfCBx/LL7QqhPevPykdZ/evlbl/jvzXzhOy7l8/3PbjbwN5+Q+HdD5+/cgjFHAsMk4YfoZbaQ8QLzw0heLFsAXZrV2how2vOc6uOet92j3Ourl2bXi4s7oGtBvUNys/Swe/+91v43uDX7v+WXngt3uKjj4U0D5pAo61SLkXsGb5w8jcP9QX/9iaI+AQQmltlLVDdo60lX47ZM62w/pnhPeGtc1+X+PX43LuB9otv16yBqi1W4Br6l7x9089sTaetsHOi+uO2759u+vzsobveej051wjuvyywVlbwYMVy+pZHHXB+lWrx9ZuUOa+BY60fr7kY6JSAq4VoOLh7KnUfzrF2XwR5ipQKSwOBW5+Gml/vpXfsV988UWuofH3aWPxVOrLLrssDmdffgUI8/VFIk8BOLNC0Rj51hHm3oQdmu0TIWDb1tDYtglDGlUsbX4c0FjxS6OC84cIcFkCzj9Pjtlu3lDAMZwaLkCPowHFTxn46el4eMLs2vV4tz1gwICS/zIvgfkXfhj84he/SOzDGpyvv/66ZNFzRANC0E9fznHS6flxvrUS6Oj8cwGumXWeYYOAlc62EdL+EyYNHWXqd7AsyP3Xv/7VDR+w7S8txnqx5sfxFOofh2+B80UmDejrr79ektaHTo+ysQYf4URd4QHIrwPEM9cOP8eCSPbzYb4Tv3nnQPk0JeBs2Ib7+quvvorj3n33nejoozvG6XwrA/cA9d/Py9KFnVJ4//pWkvD+9csc3nvvvfg88+5fysDOHxia9fMBnG+BIb+0qQmIasPeVqW+4V555RV3zNQz3+rMsCUPuTjqXJqIC+93CK85zq65PzeQ4zCBFT508sBi9Y3yY36Sn59ZRXg4sqHIvDzCeyqLLAFnIwZmced+oA2tRMBBWhsVtkNAO2SWK1xe/TPCe8OuVXht7L+4pu4HnI2kGNZuWXxT9wrOtsO2wY/D2b2ahX9vA5Zdv33EWVuBAcXvSxkqN5FmZWPtRijgEIn+AyzO2vxy61YjUrcCDrCysfg5zlfmvCHExF/8NGiY5TEp04n4wwphQ+0LOEQfDXW4T6Cy+PMCeDr1K0CYr1/p0gQUN7w1OlTeU089pcl9YkHzz9mHhpV9ckNhneEp0Yan0vaPyxJwDGdQDgxTYzGzuFDA4UjjHwfOxE9YJkCHRieH+/TTT0viKBPfymEg3M2KiuneLIsMeeL8N5jCxgaae5y4cD4MQwxhvnSeJmbCBoGGA4dYQ5hiNfP/mwbrwmIVwfkNmi8muP7hMLEJOCx9PIGH+ebBcSMGcDzNW13xh019wo4Z/Dl4WefQHAEH1EmGGvGblRA4fx6I/P+mgQs7pfD+HTz47Hg7rAOhgMNi6p9n3v3L+WP9wbE+pp8P4HhYtG0EK4I+7byYTO4/qIYWOJ/w/3YtQ2tyeL8DLu2aY4Whnofh1LUwD5+wnjBsyTnSLr/88stl5RHeU1lkCTj8/nApDwL8hqLDBFw4zAZpAi6tjcKFb1LifKGVV/+M8N4IRYrlZf8t537A2eiUQbvli8um7hUcfh480uqJEbYNIdRFy8vIE3BMFcEqyz1Af8bwqqULyyYUcMAXDrCE0tfh7OGq3LrViNStgGPugVU6hACmXIaJ2OapgEp55plnxulplBEjjJn7YVkCjicrhiDCDhLyKnVavk0JOBwNCqIz65XtcJ9mgQzT2T7ovHc9wb4WrV//rdUxa/9ZAo5hBytXGkeLCwUcljHfcmX52g0elonfSHAszPsJ/8tQjh8GPD2SFyLIfzsKywWO+W8WhiXSN/dDc48TFwq4NAsnT4cmzMIGwRdwZpny/xviT6Bn+Muf0B5a4Pz5IeBb4HbseK4kLg9/qAULEM6GL6+++qpEegg7ZjBhk3cOzRVwwL3Jgwll6KcLLbdp4JrqlCoRcLi0+9fKgHuOMggfUgBrF52WH4bz5wwa4XSAPAH3zDPPJMJwNm3BCO93S5d1zV966aVEGGQdB4T1pFevnm4ftNOUSzl5hPdUFnkCDisMFkn6BXvADwUcD7zcn/YfH+Zy+duUXVobhfPbIaAdMsspLq/+GeG9EYoUy8v+i2vqfsCFb3X6D5S4pu4VHH7KOaueQNg2hFCelpeRJ+AQ/C+88ILr1xD+1157bZwuLJtQwPGA7xs8cBJwrQjj+P7kcYaSrDNjHgXOnxyMY26JP+k2bKh9AceNifNfFbf88yp1Wr55Ai58Ussi3Kc1gn6Hxrlx82FhSntSBoYO/vKXv5TMJcOlCTjS+uJv06aNcVwo4Bha+eabb0om9fK0ZE97YZkwPO0/Lb799tuxH3Dhm2scW16jgKXDz4fh8nDY0I7TD8s7Tlwo4DDd++cO1DUbZgobBF/AMWk7/C/X0p/HRkfvW3H8OhgKONun4Qs4HkD8Bt2GV6iD4YOJPzeUOS00dliGmPMUCiyuPb9hxwwmbPLOIRRw4duRuFDA4WwOqh8WDjf6nY+fzh/CDe+lsAMN60CegMu7f0PBhqUuTIPQ8ucKAuVNuYdpmyPgECHMa/TDcP6QLqQJuPCaU1fsmlMvbD4g2PwnRjZC8WD1La2e0AEjBv16n5dHeE/xglDatU4TcP6DDMO01Dcbtg4FHPCAF74FfMop/RMvkWANDtsosHbIb5v9dgiXV/+M8N4IRYrl5Qu4pu4HHC8q+WF+G4Jr6l7B2Tb1xB7wwW9Xwmtudcjg2jO07M+Ps2kBto2ztoJ5hf6ndHzCsmFf/rx4yoXROj9fCbhWhBuK4R4aNSp62AjSaPBmmG0z+ZI3Am2bSofD8mOdHiKQTs+GTnliQujgbLI8ZlgqLZ0+w7JUEho8HE+TYb409jjbB5ZCnH06wN788V1ojbJ98h9/MjggSnE8UVpF5205e3nDHE819uRFI0l6rJS8XYpjaI/jZz84e9MVsYPj/BmyRHjZHAqevsj3gQcecNu8ucSnC7ge/hOslQmOpyjCaGjp0D7++GNX3rwtZOnpFExQhqxevTrOy5z/MsfkyZOdmOJtpnCuh8FxMuE7PE67Vlb+NjGWck8TxDSu1EEm6toQNW8w//73v3f/s+EqGimcPVAgXClPnC8eDRoURATWQjo5m2fDtedJnidQmwtFh0QePHxglaXz4K1H0nO9OD8c+7MOhbcL/YcKoAE0ywMNpT+3jrk07BfnT7C3MI4HazidMo5jzjoHKx/Kn/Khg/nwww+dMOAFE+q6lYv/tjWTkMMXWIB7GsexmFUlhLpAPII06/7lfgnvX/4blrmdI47OKu/+pQw4f9oiyiBreCvt7VCGK7E2MEcIAcLLEX49t/sel/YmM6MS/J//8ksZh6Id7H4P2xX/mvtvpjLEaHWKDtEX/oRTrjhEAvXNyg9H+VlajiXtg+Fpefj3lLXplCsOQWH/RSThaLNo1wj729/+5sJ80WlWMx4qaM9w7733bkl7gRGAtg2HhSoU6rRRXJPw+A3aIeoMdc8XVX79Zjusf34edm9wH3Fv2LXy/4vz/1vO/YAVlraLdouHcj+unHsFZ/ujniBWcdRJqytcdxx1G4snfY8/VOtj15J2kr6EdggRyXXHWVvBSwyh4yGJezAsG0CEW/9E3WPONWFcD8qVtpJRubC9bkvUrYAzGDqzp0AfGmJ/0i5vnIRv5KRBfmFDy9NemK5aUEn5ZADzhWjQ6DhpGMOnvTzCJ+0HHtjkLJTkB7wogAgJh1XsZuKzAGGeBsfBW2fW+dPw+k+WlFd4rCbSyiHtNXxM42GewBwiGgsaVzs3G+7z03Htw2sYQp1pznFmQSObdqzlwBBbWqdqUGfLydu/Jmlve6V908zmlPnw37ROFRCLadeqKco9h6buMepqKHQN6n9T97ZvJagmTd2/nD/3X14ZIA6yzo1rYi/6FIF7N+9TOHnkXXMEfxgGnGdafWsO5eRBnWcKRdrbm02Rdo+kwT2VdT/QRiH6wvCQptqhcmju9SvnfoCsOlnkXsmqJz557S3lzDWlL0p72QZo+xl2t7bfRkJ4IA3TAvmE/ZNv8SUPf7stUvcCrpGh8QnnQ3HjldMw5METlG8GB55KR48elUhbb5x//vnx20Eh3MDhxzMZJrUJyaJpEELh/BxRjGrev0yw11qo5RO+XdnSZLVRIh0sev4nYorAKFEouOjXsh4mhARczWGyMhYzPorJMBHfWWMoLEzXHHiFHRM5Qys8sTCckPYGXKPB8A3DQTz98iIBc8sYUk2zwArREtTi/hVCJOHhna9D0PbTBzCfuSnL/e6OBFwLwDAc8wT8+RyVgjmaSZoIt7S1MxsZJsfTcTa1rJQQLUEt7l8hRBKGpGn7ma+YNdQqvkUCTgghhBCiwZCAE0IIIYRoMCTghBBCCCEaDAk4IYQQQogGQwJOCCGEEKLBqFsBx5ed+S5MuD5dOfCNNNak5JMUYVwIH5TkdeUwvLnYV6TD8Fry2muvuq94p72FyvfAzIVxwAoXFkd58SV9f+1RIYQQQtQvdSvgbEkSWz6qOSDKWNpm5cqVibgQlqTJWzIlD//r1HPnzskUS7WCZUVYoshfG9DgFew8AccSURbH19xZo2758lsT6YQQQghRf9StgANbFJuP+bEGHOvF2ZJCfLF58uRJcVq+kM46eraEEMuN8JFbltq4/vrr3OLGaV9C5yOxtog0efOtJ9ZdZb1R1tLj/+F/sG6RJ441PlkvjnBbFDvvv6zNxlfZ05ZC4Viw5HE8fMyQVQjsI7asb8eixP6CwXasfh4nn3yS+wgiH8DlY7/+4ufAAs0rVqyIpk+fViLuwkWNgaVROFaugx8uhBBCiNal7gUci4yzoC1DfLgrr1zsPmLLeoS+AGHhWhyLfrONgGMIlkVtEVYsP8VX1P38WZ/wpz/9aZyPrbuJY7mchx9+2PkRZP7/EGE/+9nPXNwLLzwfL13EftasWRP895T4fyx1NWvWLPdRUJaNClcYsGPBMazLwtpbt26Nliy53p2/LQps6c3ZNuXC8bDgMEvR4J5/fkcczwdJOcbhw4e7cvH/Gwo4jpXF2TlWFrAPj1UIIYQQrUfdC7hNmzbGC4IjOj744APnxwLnCxAWlcaZxQkB9/Of/zxe2Hjs2LEl6Q2ECfPI8GOB+/LLL0vSIfzShmKxhIX5IY5GjBiR+l++LI3ItDiGP1kqyv8/x2LOwnCsH4qfRY/J0+LCY92wYUPJWnK4bdu2OT8Wv48//jiOQ8z5//UFnB2rrW3H4sfhsQohhBCi9ah7AefPgUNE+KLjrbfeKkn/pz/9yQ074rchVD9+6dIbol69eib2s2XLltiPCPvDH/4Qb7/55pvR66+/lvhPloDzt/3/Ms8Ox9wzwrEO+tYxAzHpHw8OQWXbiFh/f3aslFV4PD/4wQ9iQUtaE6rA/D0/vS/g7Fg5Po6VxeTTjlUIIYQQrcNuJeAWLlwYHX74YYn91FrA8ZYorpyXBIoKOCx9vNDg5+ULOCyX/ny5PAFX7rEKIYQQonVoaAH33HPPxX6GP3FZAo5h2M8++yyxD6i1gMO/efPm6O9//3tJ/EknnZjIt6iAO+ussxLHgzMBh/XRj88TcHasAwZ8+wkXO1aGqv19CCGEEKLlqWsBx9wv/wWCVatWloiOBQsWONFx9NEdnejBffrpJy4OAff111/HaXmb8p133knsA3wh+MADD0R/+ctf4m0EEy8lhP/hMx3ffPONeyPV/6/NuQv/e/zxXaKvvvpqp6i8NDrxxL47026K1q/fJTZ9mLfnHw+Ot29t+/PPPy/ZH8fKPvfee69o+/btrjw6djzKfUoFR36kRaDx4gVv4/LyBt/Iw3XteryL79Kls3uj1j/W3/zmN+5YeXPVjpXz4e3X8LiFEEII0XLUtYArB8SIDYsiQsJ4LG+dOx+XCK8WfCoE8RSGZ0FaxFUYXi04X77rhkWS8/YFJWBBw/pGunKOo5w0QgghhGhZGl7ACSGEEELsbkjACSGEEEI0GBJwQgghhBANhgScEEIIIUSDIQEnhBBCCNFgSMAJIYQQQjQYEnBCCCGEEA2GBJwQQgghRIMhASeEEEII0WBIwAkhhBBCNBgScEIIIYQQDYYEnBBCCCFEgyEBJ4QQQgjRYEjACSGEEEI0GBJwQgghhBANhgScEEIIIUSDIQEnhBBCCNFgSMAJIYQQQjQYEnBCCCGEEA2GBJwQQgghRIMhASeEEEII0WBIwAkhhBBCNBgScEIIIYQQDYYEnKg7Nm1cX7LdqVPH6J67V8ewTbgftn7dymju3Onxfwjb/Nj90R133JLIv1yOOOKw6PDDD02EC1FrqL8nnNAzER4ycuQ5JffB6tW3JtJUylFHdUiECSFaHwk4UVfceOPV0datj0QPPnB3HHbSSX2iYUPPjrdffGFLtGDBbPfrh9OB7bXXHtFDD94T7bPP3i6sX78To2nTJiT2c8EFM3fG9Y23Bw3qHx177NHR7NlTXR6EbdnyUPTUUw+6TtIXkffefYc7zjDPSti0cV30yCMbSsI6duwQHXjgAYm0zWGvvfaMtm171B13GNcUle67mlx66fzohee3JMLz4HouXDgnEd4cxowZXiKQ2CZ8+fIb3APC0KFnJf5TDcK6bXTpcmw0a9aUaP/993Xbl19+oUtr8Vxv6r9tk3bUqKGJfNLievQ4Pjr00IOjSZPO3Xnt93dhffv2cuXOPRD+XwjRukjAibqBzmfHc09GF110geuUzNKWJuCWXL840cmtXXuby+Ou9Std5x3mb2CleH7HUyUdHyKHbTqrO3fmY/uxMI4BP53jo4/e5/zVtExgdQwFHPulgw7TNofLLl3g8skrjywq3Xc14ViaK+CA63zooYckwstl/PjRTlzD449viubMmebEDdf/4Yfudb/jx49J/K9SwroN8+fvemgBHi4Q+KGAA46L3xkzJiXquZEWt3LlMhdGOfMQhYBfs2Z5fA+EeQghWhcJOFE3YNHA4oWVAaGEmCMc8UTHwvZzzz0RnXPOGS6cjmXb1kd3hj/h/FddeWmc13XXXuE6OcJ79+qe2Nf11y1y+2jf/nC3jYCzOP6DdQ5BZcO5JuAsDZYJrHj4bT9gFh9EItsc96JFF8X7YKiX8NNOO9UJVPsf5+0LuKOPPiqOs87T/gv4CaPT3bLl4bgsunY9Ls7DzuWSSy5wfrPGkR/HRTnSSVueBvsmje2bdPPmzYrjOV/b95NPPhinNYtdmhCwYyEcMeQfI9g15HfYsMEujLK3feK3csD6aeIDzjhjgLOg2vGSh+X77PbN0e23LU3sr7kg2tgn/hUrlsXhV199mduvn5Z92rFgqbPyoVyIx28PDM8++/jOB45VcdlYHmyHAo6wvn17O/+tt14fC3zCEZdhHrB40cXRqlU3p1qhwzg7PiBvq7dhnkKI+kACTtQN1iH7EB5a4Pz0hI8ePdz5ERlhGv7rd+AMrSKW6EARF+3bH+HCyxVwdLb3bbhzl2XnkINdHJ0dQ2smhhi2RWyOHzfaxZsQtU7bLELkNXbsyOiAA3ZZdPIscHSypGHobOrUXX7C6XTvXHu7m6uHuPWHz+xczjrrdOcfO3aEOxby4Fg4DztmrDkcF2Vi//UtcLvOfWV03HGdnOiwfRPOkDPDcQy9WTjXgnDiLRwxY3kxXGd5k47jwaK58f71cXmRDoGEhQs/5cEx2rlfesm8eP/2f64JeVjeXD8sprYNiLE8bPjdBxFs+/XnmV144flxuGFlTj74uXb33rMmFqCEca0RxvgRxDxk+PngD+v8rnPd0/mxoLHtW+AoXz8PrgPXivOfPr1UwKXFScAJ0VhIwIm6gY5iwKknxzzzzGMuvCkBh58O0ToaLCB+us2bN8Z+hIZ1cgi7pgScCSITcI8+cq8DKw+CiDj/pQvSIDrwY2XD0kcYlj72gXi0tCZUgA4zT8AhAPyOFMsXv3S6WMzwm7XHOnk7ng4d2sXbxGHB4ljABJzFc7zmDwXcqp37Yuja37edA/lah58lBLCsYvUhL98ihNWQcydvzpN4LE2cr81HpMzJq2fPb4UOdQQ//+eX/1seZqmiTML6QNo8zLrpQ120utCUgLN6CybaGHq1dPwiwCz+ysWXOL8JY0sT1nnCzPrMMD/bvoBDvJr1EaFt4Twk+AIuKy7rutk5CCHqCwk4URfQwYVDnYgrRBbiKW2yOJ2QH87QFmkPPvjAkg7ZFzS8WYrFg06JfZooShNwXbt2dhYptsMhVEvHL8NXfhgCyfKwMM6FffhvxfrCkjRpAg7Bg3/pDVeVdKQmEuh0Gc7Fb0Ose++9V0m+vPBh8Sa4GHYlj1DAmZgA27flM3jwLvFAGdq+Gba1NHkCDtFwxRUL47x8QbHvvvu48zM/ZW3HhXWScLtmWMiwNHLdEMMcE//xy4ZtG7alzBGeFlcEhvZ9UYZosvmZG+9fVzJkC36ZWP3i5RgrZ34nTRrr/Bz3ZZctcH5faJImhDI0Ky775YWDcA4cgpdrTBlYfbDhfKuPkBaXdt3wM+RNmiLzKIUQtUMCTrRJGCakw/PFm48vcsrBBNzEiec6li271nXsxBGOpYc39fAj/PgdPXpoNHPmZOdHsIQCDrGEQMXSQocZCjhEAyIFkdq//0kuH9LaxHLS0OmShiFc8gjfNkVM2D7piBE/zA/k/4iLPAFn++Z8EAWIWeabmUBpjoDr06eHE2nkx/5IY286AsfC/MH7Nqx1fqyGpMPqhTAnjLzs/1wPQMTw//vvu9P9nzDyMKsjaZn8b/spAnnwFqxt9+jRzYl26gFxq1cvL0lfDQGXh4nHcrBh/jTy4oQQ9Y8EnBBVAEsRgtEPo6NtSihimWvOW5IMQ/p5mlgyi1MIx4VoMyuUDUkWJWs/5YDlLk80YDkNw8L9IeYQwgMG9HND1MzjMxHH//3hYvaHWA3zrBY2dC2EEK2BBJwQDYxv7cpiypRxJVa2RsZNvH9k12cyEIO8CGLDrCGUDUOJYbgQQrQFJOCEaGCOPLJ9Imx3gE+NhJ9MEUKI3QkJOCGEEEKIBkMCTgghhBCiwZCAE0IIIYRoMCTgRJujW7cu7rtWrE4Qxgmxu7DnnntEgwadEvXt2ycRJ4RofCTgRJvgmGM6Rvfeszrq1atb9M///E+OfffdO5owYUw0ffrERHoh2ipPPvGA+/YeH961e8G+Qzdy5JBEeiFEYyIBJxqeffbZy328lU7KOiyfhx+6NzrvvJGJ/wnR1ujU6Sj3UefwHgCs0v7ybUKIxkYCTjQ8fGl/yfWLEh2WwVJTWs9R7A7csOTKRP33GTYsuaawEKIxkYATVYV5Ny0NyyiFHVXIkCFnJv4nRFti9Ohhbom3sO6HsHRX+N+WIGwrhBCVIQEnqsLNN18XPf30w26eTUvDWp9hJxXSo0fXxP+EaGtccsm8RN0PYSmy8H8twY7nnohmzJgUL30mhKgMCThRmPbt27n5ZTTOrblkEYuuh51UCAuPh/8Toi1xxhkDo3V3rkjU/ZDzz5+a+G9LcdaZg6IHH7jbtRlhnBCieUjAicLcvOxa1xAzPBnGtSQb7l0TDR58eqKj8rn7rjsS/xOiLXHAAftF27dvTtR9n7322jPq3bt74r8tSbt2h0f3bbgz6tSpYyJOCFE+EnCiMIi3Cy+ckwhvaYYOPds91R944P6JDgumThnnLBPh/4Roa8yYMTFR/30YYg3/0xqcfPIJ0WWXLkiECyHKRwJOFGbTxnWJsNZkzpxp0cKFc9znErp0OcZN6l679raoR49uibRCtFWuuGJhtGXLQ26o9KijOrgPW/OZndtuWxp16HBEIn1rwQMgnz0Jw4UQ5SEBJwpTj1ati3YKOIZUN2/e6F6sGKUPl4rdkJNO6uM+5ouQe+SRDdHZZ5+WSNPaIOAQl2G4EKI8JOBEYepRwAkhGgMJOCEqQwJOFEYCTghRFAk4ISpDAk4UplwBd8j++0YDunaK5p3eRwjRRpk9sFc0pHfnxP2fhQScEJUhAScKU46AG3dy12h6xz2iiUf8sxBiN+DaUzpG1wzvl2gLQiTghKgMCThRmKYE3KnHHx3dOLh3ooEXQrRtru7X9NulEnBCVIYEnChMnoDr2bFdtPTsnomGXQixezDtlB6JdsFHAk6IypCAE4XJE3CDex0XXdO/Y6JRF0LsHlwzvH+iXfCRgBOiMiTgRGHyBNyovl2iK0/qkGjUhRC7BzeNPjXRLvhIwAlRGRJwojAScEKILCTghKgtEnCiMBJwQogsJOCEqC0ScKIwEnBtiwU9D4t+9/KTCTYvmuni37/31kQchPkIARJwQtQWCThRGAm4toUJuDfX3uD44eN3lYg0BBxhFm+E+QgBEnBC1BYJOFEYCbi2hQk4P+zjjStKBNymBeMT/zNeW3VNqnXu0hOPin71/KNx2Mb54+L//PalJ6Kvnn0wjpvb9UAXPrn9v5bk8/b6G6OpR343sU9Rv0jACVFbJOBEYSTg2hYm4D66/3bHl1s3Jixw339kbfTSbYtjpnT4t/j/83scGl05sIvjNy8+Hv/v88fWOT/5r5pwuvMvPvU4F4f/q+ceipaNODn65Y5HogcXTnLhW665IPrZM5ui687qGR/HgwsnJo5Z1C8ScELUFgk4URgJuLZF2hy43778RHTf3LEuPm0O3LSOeybyAeJ+vGVDNP3ovUpEoMU9dfXc2L9hzhjn33rd/Oi1O651fsTby7dfGd0ze5QLI90nm1Ym9iPqFwk4IWqLBJwojARc2yIcQsWPJW125/3cdlNDqIBlDdGHpY3tOccfEOfj58vQrPkv79fJ+RFyr99xnfNjlXv0sunR+hnDYlaMOy2xP1G/SMAJUVsk4ERhJODaFqGA++KJu932W3fuelGhKQGHNQ7h5ecBP9/+QEIYMkRq/jQB98aa66Mdyy51/sUDOkefPbwmts6JxkACTojaIgEnCiMB17a4oPvBCfH1zl03xWHv3XNLyQsIId9/9E6X1sfiGAq1MBNswDYvOeBHwPEihMX94rmHXTy/L992ZTSp3b8k9inqFwk4IWqLBJwojAScaA5Y0hhSDcPzuLDX4Ykw0RhIwAlRWyTgRGEk4IQQWUjACVFbJOBEYSTghBBZSMAJUVsk4ERhJOCEEFlIwAlRWyTgRGEk4IQQWUjACVFbJOBEYSTghBBZSMAJUVsk4ERhJOCEEFlIwAlRWyTgRGEk4IQQWUjACVFbJOBEYSTghBBZSMAJUVsk4ERhqi3gru53VHTfhFOjS3odKmoI5bz+3JOjGwcdm7gGjcSlvQ+L7p84YOc5HZY4R1E9Fp/YPlo9ok+i/JtCAk6I2iIBJwpTbQG3bsyJ0fSj9kiEi9pw+5AeibBGYW7n/aKNkwZG87rsn4gTteGWs45PhOUhASdEbZGAE4WppoCTcGsdLux2UCKsEbh3XP9oaod/S4SL2nHjacc1656WgBOitkjAicJUU8DN7rR3IkzUnot7HpoIawQYOp3S/l8T4aJ2XHfq0dE1/TsmwrOQgBOitkjAicJIwDU+EnCiXCTghKgvJOBEYSTgGh8JOFEuEnBC1BcScKIwEnCNjwScKBcJOCHqCwk4URgJuMZHAk6UiwScEPWFBJwojARc4yMBJ8pFAk6I+kICThRGAq7xkYAT5SIBJ0R9IQEnCiMB1/hIwIlykYATor6QgBOFkYBrfCTgRLlIwAlRX0jAicJIwDU+EnCiXCTghKgvJOBEYRpZwL1y+QTHw9POcB1TWtxLl42LHpp6ejTtyO/GcZN3ioZ3r50RPTbz7JL/XNB5v+jBnWmfmjs0mtVprzgt/7c0xL933Yw4ftkZXdz/wmO7c/SJibBaIQHXNCwhZXUCtl84Ko7zww1Lf9e5J5fkc8UJR7hw237hkrHRB0tmRdvmj4jDqHOWzzMLRro6dOvZXeP4taP6Ri9eel7J/q86+ciS/dQKCTgh6gsJOFGYRhZwHy09P/avHtkn+tnqy+Ltx2cPjv3rxpxYEvfhDbOdgPPDrj2lo9tefGL7aMXQns5Pms9vnR99fOOu/ZCGePzEn3/sPs7/xe0LS47rBa9zbgkk4JrmvgmnllzvSe3+JXr7mmnO74eH6f04rrcf9vJl4+P1f2cevWf01JyhcX6INj+/H6+82P0u6tsuke+PVlyU2H+tkIATor6QgBOFaSsCDh6beVa8sLsv4Fb+Q5DZNv4bBh4TvbZoYhw2r8v+LhzrCP/Ff/OZx0dTOvxrLMhIQ7zlYZ00/qWDjnX+6R33iL5cfWnJcdUaCbimCQUcvHPtdPcbhlv6n95xSfTpsrlxGJZY0n6wZKbbNmFvvH/9rnDE2gNTTtt5XQ6J4/jf/OMPiJ6cM8QJfl/k+9a4WiMBJ0R9IQEnCtOWBBzWjeWDuzn/Z7fMi55bONqJNASVbyUzP6JryYBOcfjWecNj68g94/rH4X5nSxz5+fFYUEwMrj/3pFRBUEsk4JrGBBx1YsdFY1z9WDPyBBdHOOLMQISbgCPNNf2Pcum4zgyPvnnVFLfN8OjUfwzNz+i4p6s/lt9PVl3ifm86vXMctmp4b1f3nr5geHTLWce74VgeEFaP6JM43lohASdEfSEBJwrTlgTc7UN6RJfv7BTxmwUOseXPWWJOk4k0Y85x+0bPzB8ZD3Mxv80XYSbgSGNhxD8y/Uzn5/9sX9bncPfri8KWQAKuaXwLHHMc/eubJrhNwFk819iscSbgAOsZ9SYtj5XDernwBV0PdMIP0egPl35/53Y4D7PWSMAJUV9IwInCtBUBxxwkrBvMbWLbBBxDWXSizF9jm06UczJMiDHf7dObvh0uw4JifhNwpPHjd1x8brz9+uJJziLzw9sujMNaCgm4pvEFHNYy//qmia9QwCHUbPjcF3C8HPOD5Qtiqy5WYMQefhtynX3M3u6lFvxY3/z9hvMna40EnBD1hQScKEwjCzg6QOsEsbIt+Mf8N9g869s5cLwBaJ2wWdnCOPxY2LDYsc18OkvzvCfU0uL9OIRkGF5rJOCaJm0OHHWBIcwwHDaM/1bAMQzqp/EFHOGbJg9K5Es4byszb9LCw5cVmEuXtu9aIgEnRH0hAScK08gCrhZgwTMrXhrEMSQWhoPfWbckEnD1By8shGFh/eCt1qy6VCsk4ISoLyTgRGEk4BofCThRLhJwQtQXEnCiMBJwjY8EnCgXCTgh6gsJOFEYCbjGRwJOlIsEnBD1hQScKIwEXOMjASfKRQJOiPpCAk4UppEFHG8JXlJn4oXJ6/b2YkshAdc0fOoj7+WU1oSl2/isSdqLD9VGAk6I+kICThSmUQUcnbF9oJfvr4XxxPEFfdar9MMv6314vPj8veNOKVnA/NEZpZ8G4ZtxaXnzNf8wzMd9ob/DvyXCa4UEXD58Xob1cPHbkmdNYYvZG9svHJVIkwb1kiW6/E+NAHVi1bBezv/QtDNK8iaMj/r6S7vVCgk4IeoLCThRmEYVcIgr82Px4kv3frx1jOF3tujAL+l1mPOHnWwInSodL992Y11UC7+ox7drXAIfaeVbYf4KDC35gVYJuGxYFsuvA3wPLkwDfM6DJa1MeIffjUOYvX3NtHibtDftFHlhHv5/8LPYPd8HxMLGUlu2asd1Kat1fG/ZBYmwaiMBJ0R9IQEnCtOoAs7vKOkY6ST5wj7bCCnrpCoVcHyMFYFIPmYh2TJ3mPulU+djrPZxX3/olG2+0h/mWQsk4LKhHjQl4NaN2bV+LbBc1tzO+yUEHGBZ4xdLmtUJP566FQo4rMCsz4ugp57yIEA4+YdCyv9gdK2QgBOivpCAE4VpRAHHQuBh54mQwoIRrmOKnyWwDDpoX8DRsYK/TJaBgPMtabbUlgm4u8/rF++LJbtsYXPbb0stUi4Blw2iyV/eLE3Aca1Yxxb/k3OGOEFuAu7DG2a733Be48ZJA511duXQniXhWIKpi/zPr1Mm4PiYL2v28rDBtl9Xl53RxYnH8PiqiQScEPWFBJwoTCMKOBasDwXcbed0d2EsgUQHbOFhuuZa4Pw0L1wy1v2agOMFCvK/Z1z/xDJJhNNZhnnWAgm4bNiHXx+yBJy94IB1zSxkVneemju0pB5tmz/CCTKssyxYH+ZHOOkv9JZ2MwHnp7PhVNvmgaDWL1pIwAlRX0jAicI0ooCDz2+dnwjbPOts1yEi8CysOQKOOU5Y6GwbAUfnz3y6HReNifMyAcd6q1hm2B8vR/h5hYKulkjAZRNaZBFmDGkaDLczP41rzbq4XG+EejiEyjxHLGfUHcL5HxZfxFw4VE4eYd0yAffJTXOcnwcO9ulb9j5aen7i+KuNBJwQ9YUEnChMowo43hilc/bDGJ6ig/TDUgXcPwRP2Mkyx8l/OYIOFpFhi46bBeWpOUPdL2VDuIEgIJwOPbS21BIJuHw+WDIr9pswM7jefL6Da83264snueHNUMABi9RP6fCvTuARx5up/F7hPTBM3nk+pAvrJqLt6QuGuxdgqKP8j6Fd5sgRv/jE9on91QIJOCHqCwk4UZhGFXDMN3tmfvITH7MqOIa1o/rGb6+G+PPbDDp/RCBlhEXGxB+WQHuhoiWQgMsHC6mJ6zzCxebzqPT6zj6mtJ4yZ46h1zBdtZGAE6K+kIAThWlUAQd0olg8wvCiNHf+kVli7I1EK6tKO/fmIgHXNC19TZoDVj0+dxKG1wIJOCHqCwk4UZhGFnD1AAIS61sllr9KkYAT5SIBJ0R9IQEnCiMB1/hIwIlykYATor6QgBOFkYBrfCTgRLlIwAlRX0jAicJIwDU+EnCiXCTghKgvJOBEYSTgGh8JOFEuEnBC1BcScKIwEnCNjwScKBcJOCHqCwk4URgJuMZHAk6UiwScEPWFBJwojARc4yMBJ8pFAk6I+kICThRGAq7xkYAT5SIBJ0R9IQEnClNNAQfhwt6i9oTrbjYKdwzvHV3Y7aBEuKgdK4f2jC7ucUgiPAsJOCFqiwScKEy1BdyyM7okwkTtuLrfrsXQGxEWgV+1U8SF4aJ2rBtzUiIsDwk4IWqLBJwoTLUFHDBMc9+EAaKGbBh/SnTTacdFU9uAxXPxie3dcGp4jqJ63D9xYHTr2V0TZd8UEnBC1BYJOFGYWgg4YIHuKR3+TdSIsLwbHdaUDc9RVI+wvMtFAk6I2iIBJwpTKwEnhGh8JOCEqC0ScKIwEnBCiCwk4ISoLRJwojAScEKILCTghKgtEnCiMBJwQogsJOCEqC0ScKIweQJuxAmdJeCE2I25UQJOiJoiAScKkyfgOh52UHTLyH6JRl0IsXswrE/nRLvgIwEnRGVIwInC5Ak4mH96n2hR33aJhl0I0baZftQe0REH7Z9oE3wk4ISoDAk4UZimBNxRhx4YrRx/WqJxF0K0XSa1+5foxnN6J9qDEAk4ISpDAk4UpikBZ4w7uVu0Ytxp0dKzewkh2iiIttvHnhp1P+qIRBuQhgScEJUhAScKU66Ag7323CPqeuThQog2Suf2h0YH77dP4t7PQgJOiMqQgBOFaY6AE0IIHwk4ISpDAk4URgJOCFEUCTghKkMCThRGAk4IURQJOCEqQwJOFEYCTghRFAk4ISpDAk4URgJOCFEUCTghKkMCThRGAk4IURQJOCEqQwJOFKZcAbf/IQdGR/frEfWZMkwI0UbpNWFwdOzpJyXu/ywk4ISoDAk4UZhyBdzARTOi42eNig4fNVAI0UY5YifHThse9Tjv7Gjfgw9ItAMhEnBCVIYEnChMUwKu40ndo97zzov2HtBTCLEb0WnK0ER7ECIBJ0RlSMCJwuQJuD333jM69dKp0UGDT0407kKItk/77scm2gUfCTghKkMCThQmT8AdO/AEN5wSNupCiN2Dk+eNT7QLPhJwQlSGBJwoTJ6A63xWv6jT5CGJRl0IsXtwykWTE+2CjwScEJUhAScKIwEnhMhCAk6I2iIBJwojASeEyEICTojaIgEnCiMB19i0HzkomrL6+uiUhVNLwo+fMjwadOnMmBPOH1cSf9iQU6LJq65zcWGesOqtbYkwn+WvPhkte/GxRHhR7nh7W3TqxdMT4aJ1kYATorZIwInCSMA1Jvd//nr04I/eih744s3o6i0bors/edFtT1+71MUv3fGQ275m633Rtdvuj+766AW3jeDjF67acm+04bNXnD/Mf92HzyfCfPjPph++kQgvCvmNWXZ5Ily0LhJwQtQWCThRGAm4xgTBc/cnL0VHjjmjJAxht8/AXrGAC/8zbvmV7nft+8+5sKN2/v++z16N9j/9hJK08+67PfbzH/IDPy9fwPWdO95Z5EYtvTQOO/WiaVG7EYOisTcviq54fH3UZ3bp9wRveO7BaPGTd0cnXjBBAq5OkYATorZIwInCSMA1HggeLGdhuI8JuHn3r4jm7+T2N7a47V6zzo02/eCNWABevnld1G3aqMT/Tfx1GHWaE2pHn3dW1GXSMBd+yDn9SgSc8+/M0+W9M+zgs3d9N/Dih9bEonHFP/aPyGOoFP+QaxY4i+DiJ++RgKtTJOCEqC0ScKIwEnCNx+ibLostaMAQ6v2fvxZz8vxJsYDzMatap3GDo5teeKQkLtyHhR02pL/zz91wmxN6WNH2HdS7RMARRp4TV1zjROGZi+a4cATckmcfcH7m4PGfuz5+0Q3r+vu0/CTg6g8JOCFqiwScKIwEXOPRf+HUaOMPXo+3EWvMhTMxxosJJuCwfsHwJRcl8vHTDbhkRkmcL7B8oUdeFmYCbvZdNzvhaGmGXHuhC0fATVuzxPmPGHZqLOB4QcLP3/KTgKs/JOCEqC0ScKIwEnCNCYIH0TbyhkviMF48CAVc+D/7750f7LLg7X/GCU4MhuLJ/jvqxkujGXfeWBI+dfWSEgGHH0vbQWedWJaAG7Hk4pJjO2b8ORJwdYoEnBC1RQJOFEYCrjG5YvN6J3pg9bvbo/X/eMsUUcdQZ1MCDtZ9sCP281mRMA2/Jq4QaNdv3+T8DJnyWzIHbqcfIYj/3u+97ObLZQk45tBxnAwDM5xqc/Ik4OoPCTghaosEnCiMBFzjw8sDzFULw5sCoXfgmX0T4WkcO2FI1GXysES4ccTwAYmwpmD/oXAU9YUEnBC1RQJOFEYCTgiRhQScELVFAk4URgJOCJGFBJwQtUUCThRGAk4IkYUEnBC1RQJOFEYCTgiRhQScELVFAk4URgJOCJGFBJwQtUUCThRGAk4IkYUEnBC1RQJOFEYCTgiRRb0LuL322iOaM2datHnzxmjNmuUlcffcvdpx1/pV0eJFF0f77LO3Cx88+Ixo82P3R3fccUvUoUO7RJ4hnTp1jPMyTjqpj/s94YSeifRCNAcJOFEYCTghRBb1LuAef3yTO4aN96+Pdjz3RLR8+Q1xHOEPPXhP9NRTDzo/Im/s2BFx+PM7nnL/CfMMQazZf4x+/fq6sGFDz06kF6I5SMCJwkjAtRxHjT876j7vvKjXRRNFrVg4wS0PFpa9KEa9Czj2P3bsyJLtvn17x34LX7TooliE7XjuSRfWr9+JTsRZmn333ScaOfKc6NBDD3HbXbt2dr8m4NL2LQEnKkUCThRGAq5l6H3xxESYqA2HjxwQnXbT/Ojgc05OxInmUc8CbvTo4dGgQacmwg2OjaHTW29d4ixtMGhQf/f72GP3R5dduiA64ojD4vQdO3Zw/xk9eqjbvvHGq92vCbjrrr3c4QtECThRKRJwojAScLXniFEDo5MWTUuEi9px7LThUde55ybCRfMoKuAmTx5bE/x9TJo0NurZs7vzX3/dolikLbl+cXxsWNue3b45uv22pVGPHt1c+KGHHBxt2fKQi/ctcE0JOPKBceNGxflLwIlKkYAThZGAqz1dZo+OjpkyNBEuaseBZ/WNTrn2/ES4aB5FBVxL8dxOwWZDnrBt66PRbTvFGv60YU8E2OrVt8bbzzzzWOw/9NCD3X8WLpzrtnn5gV8NoYpaIgEnCiMBV3sk4FoeCbjqUO8CDgsbVrcRIwZHEyee645nyZIrXVya6Lpz7W0ufPz40e4t1DDN1q2PRNu2PRrNnDk5euH5XXF5Ag7LH/s1wjRCNIUEnCiMBFztkYBreSTgqkO9C7hjjz06evSRe91xwEUXXeA+LUJcmug6+OADYxEHK1YsK4lnSBYBRxyfGiEsT8CFhGmEaAoJOFEYCbjaIwHX8kjAVYd6F3BCNDoScKIwEnC1RwKu5ZGAqw4ScELUFgk4URgJuNpTawE3/5V7ogWvbSgJazf6NBd2/Pmj47DDhp/qwrrO+fbtzP1O7xPN2bEu/nbaglfvjc7dsKQkr4E3zHX/Yz9++Nj7b4j32+2Csc4/bO1il9fc59e77QtevCtOwzfw8E989BaX1wFn9i3Jr5pIwFUHCTghaosEnCiMBFztaQ0Bh5AKBdzZt18SzXvp7mjiI7eUpOUaD197pfMjuBA/frwJOH8fBw/pVxKG8Jv38j1uv/2umunCu8wa5eJmP7vW/SLc2D/+jhMGR30umVyyn2oiAVcdJOCEqC0ScKIwEnC1pyUE3KTHbo16LBifCPMFHOJp8IpLnbjCGufnQdiou66JRZcPAo78Zm5bHYedcfOF7j9Tn1zhtlkBAasbAg5MqMHpyxa43zEbrndCL8y/FkjAVQcJOCFqiwScKIwEXO1pCQFnVrL2557uwiZvXh4dOe6sEgE3c9sd7qPCCDtfYMH0p1eVWNimPH7bLgvbTsFlAm6/03pH4zbd6OKJo25MePjm+D8m4Eauvzqas+POOLz/1bNiP8IKIcj/O4w9o+QYqokEXHWQgBOitkjAicJIwNWeFhNwO8XWiDuvcqsQMN/MF3AIOxvyDIdDgW0sZDY/DqHH2q38zwScpTtkaP/YGpct4NbF4adcMztxzOQ9dM2iRHi1kICrDhJwQtQWCThRGAm42tNSAq7jxHNKxJkJOF5UCF9AGLR0nhNn+w7q7eao8Us4/zUrm+ELOKx3pCFPttMEnInFaU+tjIdsiR99z3XO32PBuGjEuqviodVaIAFXHSTghKgtEnCiMBJwtaclBNyAJXOcn2HSUMCxDmtoceMlBCxgvNjgxyGq2O510cQ4jLxNwLG+qJ8+FHBmVSMdw7SkPW/jUhfGW6ej7742Fpn7DOxVckzVRAKuOkjACVFbJOBEYSTgak+tBVw9Y5Y6n8NHDqipeAMJuOogASdEbZGAE4WRgKs9u7OAay0k4KqDBJwQtUUCThRGAq72SMC1PBJw1UECTojaIgEnCiMBV3sk4FoeCbjqIAEnRG2RgBOFkYCrPXwc95ipwxLhonY4AZfy+RLRPCTghKgtEnCiMBJwLUO/q2cmwkRt4AUJlvNqN3pQIk40Dwk4IWqLBJwojARcy8CHcfnALgu98801UTv6XTkjOm7GiMQ1EM1HAk6I2iIBJwojAddydLtgrJuXxUd0RW0YeMMFUYd/LCcmKkcCTojaIgEnCiMBJ4TIQgJOiNoiAScKIwEnhMhCAk6I2iIBJwqTJ+COHdRXb08KsRvTf8HERLvgIwEnRGVIwInC5Am4Dj07R93nnJto1IUQuwe9JgxOtAs+EnBCVIYEnChMnoDb54B9o4GLZ0T7npZcz1II0fY58oTjE+2CjwScEJUhAScKkyfgjN6TtYqAELsTHcaeEQ26cmaiLQiRgBOiMiTgRGHKEXB77b1ndOLCidHRk85x3zETQrRd2o85LTp57tjokKPbJ9qCEAk4ISpDAk4UphwBJ4QQaUjACVEZEnCiMBJwQoiiSMAJURkScKIwEnBCiKJIwAlRGRJwojAScEKIokjACVEZEnCiMBJwQoiiSMAJURkScKIwEnBCiKJIwAlRGRJwojAScEKIokjACVEZEnCiMOUKuF49BkZjR18TTZmwVgjRRpk2cW00dcLt0SEHH5FoA9KQgBOiMiTgRGHKEXCHHdouGj10TXRizxujzkctEkK0YXocd000e9r6qOvxJyfaghAJOCEqQwJOFKYpAbf//gdGE8+7OdHICyHaLn26LYmmjF+baA9CJOCEqAwJOFGYPAF3yMGHuyGVsHEXQuwe9Ol9eqJd8JGAE6IyJOBEYfIEXPeu/aOR50jACbG7MnLo5Yl2wUcCTojKkIAThckTcKf0GxOdPWBlolEXrcP/+H/23W0Iz120DrzQELYLPhJwQlSGBJwojARc4xCKnLZMeO6idZCAE6K2SMCJwkjANQ6hyGnLhOcuWgcJOCFqiwScKIwEXOOAsHnnzW9K2PHsz2LRc+uyx13Ykms3JQSRD2n6nzzW+Tfc/Vp0x4rtiTQhTz7+mftfGF4rwnMXrYMEnBC1RQJOFEYCrnFA2CCi7ln/SvTYwx9Hr7/ytduePWOpi5s359adgu7LaOqkqxOCyIc0x3YaGOdHPmGaEAm43RMJOCFqiwScKIwEXOOAsEFEHXRAtxKxQ9jNNz4WXbXoHue/5KLV0f/5/w6JXtzxi4TFbq/vdnS/nY89PRFHXi889/OSsOlTrnPhaQLuf/+vA6Onnvh+nPb5nf8lbMTQedHbb3ybxxuvfisQ2TbhyXa7w0+IXnv5t3HaObNukoCrIyTghKgtEnCiMBJwjUOegNv8yCclAm7+3Fv/kbZrNHniVc6PaPuf/+/+sd9E3Juv/T46rtOgOC9E2f77HOf8WOuyBNyiy9cn8jp/pwBDsOG/+MJV7lj8/+F//ZXfRYsuW++2OW7CDjukVzRk8PmxPzx30TpIwAlRWyTgRGEk4BqHPAG3fduPSwQc89p84fTW63+M/tf/2D9Oj+D6VlB9ayHr0e0cl/+wc+bGVjTC0wQcFjc/jHl1HTv0c2Evv/CrOBz/8Z3PjPd39pkznf9f/s/hbhsmnHeFAz9DweG5i9ZBAk6I2iIBJwojAdc4mADyBRyizESbL+AO2LdzyTCmL7TyBBzbr7z4K2ch8/+XJuBeev6XiTDL47GHP4q38c+dvSyOs/B99uzktjnOMaMuiunZfUji3EXrIAEnRG2RgBOFkYBrHEwAmYBj/tgTm7/nhiSxZvkCbtXt25x/471vRCtv2xr16Tm8RGCZgEM8gR83afzi2J8n4EIrH8Oh/JcwLH7/+k9HOMgfy57l6edhc+5s+9GHPnLDsOG5i9ZBAk6I2iIBJwojAdc4+KIKUcQQ5ppVz8biZ/Hld8UCDhFkaY1rrtwQ52ECrvvxZ8cvO1xw/i3usySWftCASfELCAjFUHyB/zLEtCnXxuHLb34yDkdEWnhaHqNGLIjTzph6/a58U85ftDwScELUFgk4URgJuMYhFD55IPAQXwg5uP3WLe5lhTBdGh2OONG9xBCGZ4ElMJyXB1jf/ul/H5oIT6NTx1PdG7K2HZ67aB0k4ISoLRJwojAScI1DKHryMIvWtqe/iF82uGrxvYl09Up47qJ1kIATorZIwInCSMA1DqHIyYOhUVZkYI7Zc9t/Gj1w/7uJNPVMeO6idZCAE6K2SMCJwkjACSGykIATorZIwIn/v733/rLiyO/+/4Xv89jrXWmllQgCBCNyEDkLIQQICSEQQSSRRM4gJIEICoACEkEEgRBBgDJBCBR37Q1eb/buPs9+vd5dr9dr+/g4fO0fHI7d33nV1aepW933zkzP3Ll3Zt73nNe53dXVlbq66t2f6q7KjAScEKIQEnBClBYJOJEZCTghRCEk4IQoLRJwIjMScEKIQkjACVFaJOBEZiTghBCFkIATorRIwInMSMAJIQohASdEaZGAE5mRgBNCFEICTojSIgEnMiMBVxmMHLQ7mjP1ZLR+5WVRhHUrPogWzXkzUX6iNEjACVFaJOBEZiTgys+AXtuju0e+lHAXhVm64J3ontH7Eu6iYZGAE6K0SMCJzEjAlZ+JY19JuInijLtzb7Rw9rmEu2hYJOCEKC0ScCIzEnDlZ+aU1xNuojiDbn8qWrPsUsJdNCwScEKUFgk4kRkJuPIz68ETCTdRHATc2uUScKVGAk6I0iIBJzIjAVd+JODqjgRc4yABJ0RpkYATmZGAKz8ScHVHAq5xkIATorRIwInMSMCVHwm4uiMB1zhIwAlRWiTgRGYk4MqPBFzdkYBrHCTghCgtEnAiM6UQcONG7Y2WLXg3WrawebNozrlo8oTDifzXFQm4utNQAk51tTgScEKUFgk4kZmGFnAPTT7uOsQ7Bu9u9owZsSeaO/1UNGzAs4lyqAsScHWnIQTczCnHo6Utra72fyZRDsWQgBOitEjAicw0tIC7Y8hzCbfmztL570R3Dd+TcK8t9RVw//v/adtkCfNSWxpCwCFsQrfmTq6uvphwL4QEnBClRQJOZKYhBVxdn+6bC9MnHYum3Hsk4V5bJODqTn0FXH2tpk2VGQ+8VqehVAk4IUqLBJzITEMKuBEDdyXcWgJTJx6t5tWEe22pr4C7/sudo8UL9kYv7Pw8Gj9muRNGT2+9nKBt6wHuPxRRX/5Sx2jb5gtu+/qvdImmTdkS7X3hW9FtHUc5tz/4X7dEQwc95PzMnLYjPm/T+jN54d8zdmVeuJzDuWzfdEOvaNO6M9G949e4/R5dx1YzLpGX2lJfATdiUMusq9PuPxo9eF/t66oEnBClRQJOZKYcAi4UEJVOmP6Qcgu4Iwd+FL195vfR0YM/cf8Pz9oV7Xnujx3nTv91vN2t893ueN/e9+flb/L9jzl3th/f+KY7Z8umd6Kzp37rBN1996x1x1+qDoP/xzecc37xd+r4X8bhT5ywPg7zltYD3fE//N/t3D5hce6G1SejebN3u+3RdyxM5KW2NIaAC+tBUyHMh48EnBCVhQScyIwEXM2E6Q8pt4BDDE1/cKtL67PbrzghZ2lH3Nm2CbjQCnfs8M9iAYcgMysZbncMfzh6/egvnKDDbc2K16I33/gbt41AW7HkUKK8/ugPb3VpwJ8JuN1Pf+xEHAKOcJ/Y+JYEXIkI8+EjASdEZSEBJzJTLgG3aP5L7h8xQafud0AM6e3YcjHet+E9Y/Xyo4nhOoNhvdCtvoTpDym3gKN8nCh69K1oYL8peWlPE3DwyPyXnRvlj2gzAQcMwyK2lqeIs9eO/Dzat+c7bhsBd+Dl70aL5u1xmB/C6t1jQjRl0uOxgINTx38ZX+vWN/drFgKOvHbqMCLhXhcm3fdo9Pyzn0X7Xvx2dXm1d27+8PSch56Nqm4dmTivEGE+fCTghKgsJOBEZsol4A7t+4H7N0Hhd0D+kJ758Y9jEVq36nii44KWKOBI46AB05ygoqx418zSnibgGAp94/VfRzfd2NvtL5j7Ql4Z8+4b4g0R16/PpNgdyxpxdO50p9tnGysbwgxw69ltfLVwe8JttxQBVxdxlQZhHD30UyfgGIrGzYanuVaUsVk9a0OYDx8JOCEqCwk4kZlKEHB0WogJv0Ozjsz2/Q6qmICjEzQhs3PHVefGkKAJRYbycNu49rR7Ud8EzNbH34/9sP2lP+gQhxmmP6TcAo78tr9liEvrhHGr8sorTcCxjZhCxJml09yfevKD2ApE+SAssIhaufplnTaEOmPqNif+4MVdX3fHscZZnE1NwGFxtHqxfPFB94+YGnvXsrjcrO6aP4QvZc25Vq/svUPzA1bHDu3/QfSVL3WqsWy5Xv6+QXz+fpgPHwk4ISoLCTiRmUoQcHeOnB8P6QGWh1kznor3fUECxQQcfqdO3lwt2nIv3uPGMB/vbvniBgHHtgkftufPfd6JFbbnztoZhxmmP6TcAo70vvrKj53l68Sxv8gTWoUEHB86sG3iytz5x4qJJY1tyu3AS99123x8MHLYXAd+00SGT3OwwCHCWn2tT3TD9d2dcLu13bDoui/fFr38/Dfj8kLAtW872D0wsM05vDNo4m/2Q884vwz7Wx275+6VcR3DD2XJw4WJurSyxR8fldg+Q7czp+9w6eL/rlGPOPcwHz4ScEJUFhJwIjOVIOAQFlgX6OzpALGI0SH5HZffkRUTcDaUB4ga26aDZWjRwjIBZ8fpMG3KC0QP1ic7FqY/pNwCbtjgma78yM/xV/9v3lemvoDr2nlMnGcEif+xg7kzhQiCgH3ev8IyxHaIlVnae3JGmoBbt+p1t40oQriHeaktjSngLP0MZ9q2lStlQZ198IHNrnz9/HOuXwef2fZhXMfA6liHdkOdaObBhQ9KsIAWEnCUm+3zgYmJRP6ffOxd5x7mw0cCTojKQgKumpMnjjiOHd0fLVmyIM/NB/du3bpEy5cvis/dt++56Pz7Z6KpUyfFbp07Vzn3148fzHO/445h0fnzZ6NXXtmTSINRVdUpjsvSQfjdu3d1+6QxTFe7dm0T4TQGlSLg6NyxlNkwkS/gzNoBw4fMcuf4U1b4MJxknSRhtmszyPnnRXAEg4kPBBxix85DtGAZYRsr1sG934uPhekPKbeAC8ugKRHmpbaUQ8DxoYFthwKOj0cGD5ju3KhzWHY5138nc9kjB+I6BtQx3ifEWmxuhLdk0f6EgGPOPIZjbd+nJQ+h7t61PXrv3VPR4sXz3H6fPr3iNvXgwT3Ro4+uTpyTRuvWraJly3L9RiHoE0K32vD1zy8n3IQwJOBuyt0kb791Mvrk4/OuUdm9e4fbh88/+yDenjVrevTZpxejAwdedOd16NDO+X/n7VPuf/782c7Nwvm02i//+B09eqTbPnf2uItvyJCBiXRAr17d43Pmzp0RffLJBRc38Xbo0D4688YxJwLxY+lq27ZNIpzGoFIEnA3pmcDyBdwDEze5Ybz7790QnTn5GwdDSXRyZtExEGJYjhjCIiw6SP7pPHmfi+2v/FFVQsDhTgfN5LMmEPkfNaJmK5EEXHbCvNSWShNwWM3wyzAmdXDl0sMJAcewtNUxplGxhxDqMw8YfP1LeHw4Qt1mSPbu0UviuozVNCw/8D80gTAfPs1JwLVqdZOL//3qh2P+t297PG6jL19+O7p69T23PWbMnYlzQxBw/kN9GrT7oVttkIATxZCAq+bixXPxNhYuE1Bw6dJb8fbChXPcvwk435L29NNb3Hm4ffbppTx3/q9ceTd64/Sx2B2rmm37+AIOC17fvr0TfkYMH5KXxnJRLgFXV7CqYdnw3XZsueSGrkK/EL4UfuNXeyT8hDDcx1Cr7WOJ4wvDMP0h5RZwLZHGEnB1hdUuQjcfq2P2oYjx1a90de8Fhv6zEObDpzkJONrVA/ufz3MzATdt6gOxGw/wJ08czvOHHx6orQ32BRwjNLjDpk1rnNvVKzkxGIqx9etXxn4RjbghLBF75m7ntGp1c7zPP+mytODG+YMG9csLXzR/JOCqwcrFE9ixowfcDYFVy475As4wAXfkyN7YDXM75+LG05vvzj838auv7ovdfdHo4wu4CRPudts8JXbpclvsRwKu/vDOkP+1aENjX7GG6Q8plYAL09NSCcsFKlXAVQJhPnyak4AD4meUZNKk+9x+moCjL/Dbc+Ahn/9Zs6ZGQ4cOzBNwR6uPzZ493Y2WEJZZ8EILXL9+fdxxHtLZpg/CffXqJc6d12IY0TEBx/b8+bOckCPNFgbnt2/fzp3vGwhEy0AC7iZ7orrkrGQ7tj+R975CYwg4bkSextj2BRzsfHare7riiQvhhlsxAffxx+dLQhgPNGUB578QXkrC9IdIwJWWsFxAAq4wYT58GkvA0X6G7U99efnlXYl4pkyZWC16jrp0HDr0UqqA4324jz56P+88rG+8ZjNp0gS37ws4f/SFsGwEJhRwGzasysV7cI97F8/6GUSYWdfABNxjj61z1j0LF9FGGP75/nmiZSABd1NhaxgUE3BPPvmoe/GV7bNnXnM3Nm7cYL47/9zA1hDwFMW7bBYeDYYJMmtE2OZdOvPDubz7xnYxAdeYNLaACzubpkKYD59yC7g1K4651RFuuK5r7Mb8bjaTfzi5MW6D+k/Nc2OKFVtiq22r/onlttjna9cwbh+GtL92Q894f/GCl90Xlv6C92Av5/PuI+9C8p6XnUMczEXnhxuWC5RKwIV5asqEeYPGEnCNAQ/WAwbkhhz5AIG0pAk49leuXBzvI6KwrrH90ENTos2bN+QJOGuj7dwXXnjabfujOjB58kR3nH/2bYTl+eeeymvbTcCNH3eXs7wh0njgtzD880eNGpEXh2j+SMDdlF3ADR06OLpw4Zx7N46b7siRfc6NbdxffPGZ+GZcs2ap2543b2Z06uSr8ZdPYOZ2zOR8FWXnYKrfs+dZZzrHzZ7mJOCaFmE+fMop4HifiqknEGy8NG/vVvHSO9NeMLeYTS9hcIyJeX03m4aEbX++OP+ch6ZtT8Qf+rFh5zF3PuL2mauMSZn5YIR9ttevPuH82Mv5/lQnzJkWxh2WC7RkAVdby3OYN2hOAo4H7A8+eCtatWqJezhGGJmA27Vzm3sQR3SFlrNu3Tq7dpk2mFGTtWuX5wm4LZs3uhkIEHeENWhQf+dO+LTjFg7n2KjPxInj3QgN7vbaDLMX8D61CTiEICKONLZp0zoOg/Pnzn3InU+fE+ZTNG8k4BqA3r17JtwAM3foxtMTFrjQHQYM6OtuSt+NdyHsiavSaGwBZx10+JEBbsxIH7rxRZ9vWUoTCWzzAjjbzLXFP50cE8+yzYSxfhz885Wghcn8Xf56qywlhR/EjwmgMB8+pRZwlh5edPfLB/yX4f288mUjk8amLfPEdC2hSOILXZsAuJiAQ4jZPHEQfgVM2HxpSVj+6hqs0BCGadeWtJpbcxRw1NO0a5eVcEWMQoR5g+Yk4IyePbsn3GpDrh2/KeEOtPt1abPNoufDqzT+PkINUTd+/Bg3xMy+HevRo1vifNEykIATmSmXgLP1MuGmG3rF4srvgGypK6ZYMDc7n2E35r+yqR1wR4SwzRBc7573xiLDRA0dH4KHKRyYDBg39rFgXf/lznEc991zbRWHYp2hUUoBx9xiDDmSngUPv+jy5afLB0HEeqVs45/y4d9WTjAoMwQr06mwj5WMqVpCAYcfwwSchYF1D//hV5c27x6ixXdPE3AmmlctezUu/0oRcNQBlmBj1QVzY9qPEUPnxPuUG/lnehsmT0awco6fdqAsOMYchv5EvHyAM2Tg9KhL1eg8/wxD+/FA65v7umFvvmL1BRxhE7/t8+DCXHMMXYd5g+Yo4JoC9sGDPdxjbbt48c2EP9HykIATmSmHgKMDopNm/io6HIbvnnvm0zwB9/iGc+6fd6LCDh2RgFgJRQLg17fo+AKOWfFN1CFScPOtSazdSednS0cZCKAwHz6lFHBQzAIHTPwalpFhy4f506OY6GXIFRFngiAUcH447JuAQ0Qyj14YFyDsbN1URIe5hwLu2e1X8srY5uQrt4CjnIgfwYV10OaBw7LIf7gcGxP4IsSoMyyVRX0OF56nnjIJL2XNuawIgjv+mKCXlUdwx1oaLvuGSGRCah44KE+uHe7EgxuCkDkRrc4jOCn7saOXJvIGEnDlZcKEcXkTwwshAScyUy4BB3TiTHxKh+QLODo6Ojc6U2a0D2eap9PifCY+9d0B65tvoSBsfwgVkcZxrHtYM3BjXUqbuJf0IWBYtQFLCC/ZIzDDfPiUW8BxzLdoIgrM0rNo3h533P+4wAQc7rbYOvu1EXCIhULTt5AG/DGczfXzV9DwBZxZXFmH1WAfMVRuAce8gmH8BpNNh8ux2bGFD78Yb4fnI+AQYmwj9KiDbDPP4OIFe91yZJxj6//6y74hCvm39VRv73Wf28fCxj/nA9u8CoCAQwDiN8wbSMAJUVlIwInMlEvA0cnQ6fD1JGty+gKOxeg55uMPPWHlQVSFVh4DIWEdJueagEMw8s8akrgjGPwls0iHda6GWULCfPiUWsAhaAcNmJa3rqjB8Cpff9oi86w8geUIgcrwHOLXrEeGCTizhNoan7URcGY5Y8gQWNzd/CDatnzxwcSk+x7NC8MXcIidMHwT9P4QuRGWC5RKwC2a/1IibYBYQ7hiXbTjvoBjeNu2w/P9d+Aoa6tj1GHqn8Hi9rZGr8Vj9c+W6cJCzD7Wa/7981nhAQE39q5lzm+YN5CAE6KykIATmSmXgKOD4YtEtumUfAFHx4QFzO8E6exseCpNJNApMoxER4fVDCuPheW/A4elzTrHW1oPdP+8E2eWKN5hIg4EEBYRE0BhPnxKLeCyQv5Ct6ZIWC5QKgHHO5FccyzD1EHqgi3HhiAOl2OzNGYRcPjD+sbHJmzbMm7+sm/EYe8bIswYNrUhWj7mmTZlixPcHGfYVwJOiKaFBJzITGMLOESRCTgsRutWHXfbvoBDiIVTJZios3VQzZ1hOhtqtY8eEHvheWybcKODtrnH6HjpEIF5y3DjBXLiwS/nMC9amA+fShVwzYWwXKBUAo747EMM6sQLOz93bibweTeTr5t7dB2bJ+AQYbadJuDsIw1fwM2ctiOurzYEG8bDNvWdc0gPFkKzGvPBCsfBHlIQcFa3w7yBBJwQlYUEnMhMYws4v2NrSoT58CmVgBOFKaWAay6EeQMJOCEqCwk4kRkJuNoR5sNHAq7xkYCrmTBvIAEnRGUhAScy09gCrjkiAdf4lErANXck4ISoLCTgRGYk4OqPBFzjIwGXDQk4ISoLCTiRmYYUcMMH7ky4tQQQcHXpFEMk4OpOvQVcC33YyAm4Iwn3QkjACVFaJOBEZhpSwMH0SccSbs2dNcsuRgN6bU+41xYJuLpTXwEH01pgXV299GLCrRgScEKUFgk4kZmGFnDLFrzrnvJD9+bKuDv3RvfctT/hXhck4OpOQwi4ZQtbVl0dT10dvS/hXgwJOCFKiwScyExDC7jBfZ+OZk896YRccwdrxuKH30qUQV2RgKs7DSHgVFdrRgJOiNIiAScy09ACTtQdCbi60xACTtSMBJwQpUUCTmRGAq78zHjgtYSbKM7gagG3eumFhLtoWCTghCgtEnAiMxJwlcED9xxKuInCYH27c9gLCXfRsEjACVFaJOBEZiTgKoOl89+JHp5xOrpr+IuiBvjwYOLdBxJlKBoeCTghSosEnMiMBJwQohAScEKUFgk4kRkJOCFEISTghCgtEnAiMxJwQohCSMAJUVok4ERmJOCEEIWQgBOitEjAicxIwAkhCiEBJ0RpkYATmZGAE0IUQgJOiNIiAScyIwEnhCiEBJwQpUUCTmRGAk4IUQgJOCFKiwScyExxATdLAk6IFowEnBClRQJOZKaYgBs+bEI0fdLRRKMuhGgZzJn5TKJd8JGAE6J+SMCJzBQTcDBr+tZowl37Ew27EKJ5M7D3jqhL5x6JNsFHAk6I+iEBJzJTk4Dr3q1vtGLJa4nGXQjRvJk740iiPQiRgBOifkjAiczUJOCgf787okl6F06IFsOC2a9G48fOS7QFIRJwQtQPCTiRmdoIOKNduw7RgP53CiGaKX1vHxF1ruqWuPcLIQEnRP2QgBOZqYuAE0IIHwk4IeqHBJzIjAScECIrEnBC1A8JOJEZCTghRFYk4ISoHxJwIjMScEKIrEjACVE/JOBEZiTghBBZkYATon5IwInMSMAJIbIiASdE/ZCAE5mprYC7pc3Xojv73RItnXCrEKKZsnB8h+jeoe0S938hJOCEqB8ScCIztRVwLy3qHD09vV20ZuwNQohmyrpxN0bbprSJFo7r4B7awnYgRAJOiPohAScyU5OAGzOgbbR79q3R9H5/IIRoQTw5qVWiPQiRgBOifkjAiczUJOB2z7stemTkdYnGXQjR/BnWp22iTfCRgBOifkjAicwUE3BDe7eNnn2ofaJRF0K0DBbf0yHRLvhIwAlRPyTgRGaKCbiZoztEj917U6JRF0K0DJ6ff1uiXfCRgBOifkjAicxIwAkhCiEBJ0RpkYATmZGAa1q88EifmK0PdY5mDvxSwo8QDYUEnBClRQJOZEYCrmnx/31nSx7/8u3NCT9CNBQScEKUFgk4kRkJuKYFou03l1dFf3by4ej3n2xw+0vu0jUSpUECTojSIgEnMiMB17RAsJ3eMjre/82Hq6MLu++LPj0wNc8yt/jOr+X8f3tLPMxqx9heMvqm6NcfrHLbB1YNjI/90zefiJ59uEfsn/DdsW/nzvPT4fPLiyuiV9YMdv78sMz/3366Me/YX5xf7tzX3HtL9I9//OYYAK8AAEIzSURBVHjs/ub2u/PiEeVFAk6I0iIBJzIjAde0QOT8n3eWRFdfmuxEEPvHHh3u/vcu7x8tH9sq+tlbj0S/eG9p7H/3gl7xNrB94olR0aXnJ0YPD7vOCS1EIJY8jiOofP+XX7g/evzBjnnpYB8OrxsSx004bCMOCc/isrBI7+ZpVXnpQPixvWp8m2j/ygHxdphvUR4k4IQoLRJwIjMScE0LBM4PTs1zVrgj64dGK6oF268urcwTS+bPxBLHX1kzOHp/54Todx+ti5aOuTn6h2885vxd2fNALKh83twx1v0Xe8fux2cXOj8z+v9hHOefnZgbH2f74nMT3bZvjfvRG/Od3x2zuybihZ+/vTgRlygPEnBClBYJOJEZCbimBQLHH0KF75182LmH/hieRDghwn5YLZpWjmsdfbp/qrPemX+z3iEGfVZPaOvcfeHlc2rzne74P3/rmsBj34ZGgW2EINtpAm7RqBtzou3b+fHzdW0YnygPEnBClBYJOJEZCbimRZqAmzfsOmdR++2VNU7MIdguPnefO3Z265jYsuWH4e//5NwiJ8SwmOHO+QvvuKGogDN/DNfCx3unRD97e7Fz/251OKTHjyNNwLF9bvvdbptzzaL30tJ+ifhEeZCAE6K0SMCJzEjANS0QOFi/QveFI3OCyyxa5o7VLRRsbDOUGoYLDLf6HzEUE3A+Ft43j86M3RB25r+QgINDa3Pv0UEoTkV5kYATorRIwInMSMA1H/h44NEHOsTvpBmIOP/DALaxsPl+Zg/+sntnLgwzC09M7RTNGfLlhHsxSHeYJlF+JOCEKC0ScCIzEnBCiEJIwAlRWiTgRGYk4IQQhZCAE6K0SMCJzEjACSEKIQEnRGmRgBOZkYATQhRCAk6I0iIBJzIjASeEKIQEnBClRQJOZEYCTghRCAk4IUqLBJzIjAScEKIQEnBClBYJOJGZhhZwOx5sEx1a1DF6ZOR1LZ6Vd3012vtwh2j2oC8lyink8COdoment02E0RJ5/L6bo9eW1m5OulcXd4o2VdfRMAyRD3XxuZm3RHMH/1GiDIshASdEaZGAE5lpaAGHYAndWjJbH2gVvTCrXcI9ZOOEryXcWjJzqoUGoiN091k79sbooQH5kxaL4uyZ0z7hVgwJOCFKiwScyExDCzie9kO3ls7hRR2jxUXKZd24GxNuomaxsX+eHhbqysEFt0ZLR12fcC+EBJwQpUUCTmSmIQXc/GF1Wz6ppbBjSuvo8Yk3J9yNZ6a1TbiJP4hOreyccDNmDfxS9Pry2xLuojhYhLdMapVwL4QEnBClRQJOZEYCrvRIwGVDAq7hkYATorKQgBOZkYArPRJw2ZCAa3gk4ISoLCTgRGYaW8DxdeHXn+nruPpkr8TxMLz5Q6+FWZuvOUOeerBNHN+nO/pEJ1ekd/rmx3hzXdfowy29Eu4rRn814fbuxu6J8HzqI+B2Tm+biG/fvA6J+JfckXvHjq8Mz1Wn/TvP9Y/2zM7/eOL8Yz2iy5t7xvt8wfm9PQOiy1uuua0dd6Mrpx++PCj+iCCM/8QXZbj7oVuib+7q5+Kry7UhHT94aWD08NDiX0Q2pIC79ESP6NvP9ctzK1SG5OVPn+/v3Cl/8//ZU32ijffkPjbhn322Z/TLXWPOqe3Xs+VCAk6IykICTmSmsQUcnehfHR8RffeF/tH/e2SoE0qhH+PPXhyQ1+meXtUl4acm6OR/+/pIF99PDwx226EfsDQZiIzfvDY8+uWrw/LcV425wf3/30ND43ARB2F4PvURcHzBShy/PjbcpYftVxbc6uImP+yz/avq4wiJP9+fy2NaXtk/MP/ai/+E9/G23u46zB2SE1P4wZ1yJ05E3Ld294vhOAKPDzPYpnz4/8m+QYm0p7H8zuuj/3NwiBM//Bf7irQhBRxphKentslzo5x++PLAuAxx/2R7HyferAwpV9LCNqKVfQsP/whqthFwYZlXGhJwQlQWEnAF6NOnV3TyxBHHqROHoxdeeDrhx6d3755R69at4v1evbon/Gx+Yn3UrVvneH/Xzm3RK6/siePx4XhVp47Re++eig4deikvnLNnXovee++NqEOHdok4GpNyCLgfe539Xx4d5v7pzOn8EFLbJuc6GOskccNaZ9vff2lgItw1d9+Q2nmagLP9l+e2j558INmB+WkyEDKFrGvHllalxpdGfQScgaULcWb7xE24bC8a8RW3v2TUde4/TVBSZpQF5RQeY+oXhBXbf/xs39gd6xzixvf781eGxPFjwWMbEcZ18f0deaST84PFkmtsFjpfsHEcQeSf59NQAu6h/n/oruPRJbk0mXtaGSLOZnpppDyWVpcr18/Syz5fFVtY+Oc8timTh78Qwz5cPyy6obulw9gwPvdFMtvUPz+99lCBG+miTJl3kbLHDTFNPsLwfZqbgNu9a7trXxcvnuf2aXeHDh0cH9+7d1eef4536NA+r51+9NHVUefOVYmwr1x5N+GWlc2bN0Rf//xywl0ICbgCjB490jUwb7910okltnv2TIoyg+OcY/tpN9zIkUOjN04fc9vz589256xZs9TFARYf4OfIkX3RsaP7o48+et+JOdw2blwVffLJBdfwXLz4ZtS2bZtEPI1FOQQclh2sOFg4bCoIrB90inTMJiJCC9wbBSxwry2rch00nRjbfppNwDF0SgdI3HR8hnXWiIwza7o4TJjQWSIsSStg+bJwK0HAIWrJl4lf3Ekn2x9t7R0LVSZw9UWChcEQK+Xhu/nD2lwHX9gyvGplwDkvedN8mPXKh3KkDAtZ5xAevlgKaSgBRz4RqFgZfaFpZXi2+pr7ZWggdkP/Jjh9AWcwdx3h+G6kkzqJO+WAiPSPk64/2dkvWjj8K25IGYso7tRV/nngwOpr8SOMKTOr09wfvzg81J1PHfbraBrNScC1anWTi//998+4/+3bHnf/PGRzvG/f3m6/fftrD8mffXrJ/eN++fLb0Ttvn3LbtMdh+Fevvpdwy8qTTz6a2p8IIQFXABNwtj9q1PBo3boVbpubCUzULV++yG0Dljg7xv+UKRPzwj1//mx1Q3Ax+vyzD6Lu3bvmHfPj8yH8AQP6uqc/3w/h8xQZ+m8syiHgfFFAR8SwHgKAbTo5hvQ4VlsBB7W1wBmIEawdWK3Yb8oWuDTo5BkGnFd9TfD79vrcUDXWIXuHzcBCZaLZBAT4Am7B8JyFyo8fcWH7oYAzyxsiIywnhs1DtzQaQsCt/qJe+Ni7gcXKkLQzTE6+zQ3/hQQc6UHsFbpfClngEI+Uf+hO2FjUsJxaOfv3AsO8pBHRaA8vXIM067JPcxJwCLQD+5/Pc3v9+MFYjF269FZ08sRhB/s8KPOwzTbpnjb1gfi8Ll1ui/0ZCDjrA/w229wQgIMG9cvzS59g/cW5s8fjPoVtE3DHjh6Iw3z9+CHndvDgnujT6j6lUP8hmi8ScAUwAcdNgtWMm8usYBcunHPDpTQCmzatcW74rckCB/369XHHsMCFx9JuQMSeuVdVdcrzwxPhiy8+k+d/7twZJSFMF1SCgDu3tqvrfOiMcPMFHO94md+GFHAhzUHAYZ0hzctH5z4+sOFirGxmWcPig0WIYVN/ONOGRq3sgWuCdYhtPlTw8+ven9ueE3trxibL/kp1fGZ5c8OK3gcL+OUdOt9/Gg0h4D7Y3NOlleW2DBsWTitDQHxiMfPFm/lPE3CkBaFF2GFYRiEBx/WxVwaAa7ds1PXRiS/ytmtG21jAcR1tqPYvqsUddZb0cE1JM9eL9xTDOHwaS8CFbU9DEcZD/AifSZPuc/urVy9xbjyU8885tPutWt0cPfjg/e7h3M7zBRyEFjf2x4wZ5foJRlEQebT9iESseghFG42hHSdszjE34nj66S3xSA19xpIlC9w21kPSbH0BAo5+asCAnCAULQcJuAKEFjhuYns642bixsb8zs2JW20FHGZ3Gg3OD9+TSxNwQNzc2LURcI1JOQQcHZ9hS2/ZUB5WBeCFcOtIraNkyNPOM8tZTdRXwPlp9cMJ36cqRkMIOIRUKOC2p4gPvrql/DiOBQk3G5bGzbeU2btTYO+oYREy9zOrc4KZYW726fztXMIkTRam/7VmMfw4AaEX+jEaQsARR/jeH268J5hWhnwo46cPTHyyjaBi296ZQ7SF/sOvf2vCf//TroNfPvzz1SvD4z/aO8jtI0wRc8SPmONcvjy2L2kL0VgCrrHhXTZrV2njc+8l5yxqPEAfqhZIPLSb/zQBh5XM3/cF3dixdzkxt/PZrS4cH96fox03McnrMpzjx7d///OuP+FVHv798zmOgPPjFi0HCbgChAIObJ+blaegqqqOeQLu7rtHx34LCTj8zZgx2R1/993TiWO2zbApH0/Y/uXL77h/hJ/v/7HH1iXiaCwaW8AVgs6otqKsqdEQAq6upE3REZYvw6yIZMSQ7861wAoUnp9GeG5D0hACrqnAtfHLknvJvgw2zILH+27h+bW9Ds1JwPHxgVmsli3LWbbYps1FSM2d+5Dbx2JmAsvODQUcbfDKlYvzwk8TcJMnT3TghkVu1KgRbpvweT2GNt4EHHHY+3cMt9JfPP/cU3n9ChY9/iXgWi4ScAUwAbdw4RzHnj3PRuffP+OOYUXjpnzj9NHo4sVzzpKG3+OvHYy6devi/HAjTp06KRo0qH8cJu88mOBbu3aZO4eGxI77jcSYMXfGopCXWPfte865Yyrn61W+nOJm5t24MO2NRaUIuOZMOQRcc6AlCbjakDYEW1eak4BjloEPPngrWrVqiWur7cF4/fqVLl1t2rR2+/PmzXT7oYCjDV60aE788VkYfpqAYziVr1MRh1evvBdb0BCMtPOEQzoQdmwTNtZA3GjrrU+YMOHuaP68WbGYk4BruUjAZaBdu7ZRjx7dEu6lAAFIfL4b+zRAod/GRgKu9EjAZUMCLp9HRtbfQt2cBJyRNrPAkCED8/axdDVkex+GxWgO71OH/rDKhW5AenitJnQXLQ8JOJEZCbjSIwGXDQm4hqc5CjghmjIScCIzDSng7CtSkc+z09vGSzClEb5IL3K8vrz4slSnVxUWeCKdp6a2iR67r/b3tAScEKVFAk5kpiEFHNgUFuIaTDkSTknhYxO1inyen5mbsqMQ/kTConYw1UxdhmIl4IQoLRJwIjMNLeAOLeooS5wH1rdnazFE+mQdhrVaAoiMmqbE4AvaYsJYJLFpUGqLBJwQpUUCTmSmoQUcMM0B1pOWjr9wem1YN+7GRBgtjeceuiXafP/N0dxaPgTMG/pHTvyyXFgYlrgG71mG8+HVBgk4IUqLBJzITCkEnBCieSABJ0RpkYATmZGAE0IUQgJOiNIiAScyU0zATRzWLm/5JCFEy2LbzKpEu+AjASdE/ZCAE5kpJuAG9mgd7Zp9a6JRF0K0DOaN7ZBoF3wk4ISoHxJwIjPFBBwM690memFep0TDLoRo3ux4MH/1mDQk4ISoHxJwIjM1CTi4d2i7aOVdmt9NiJbCU9PbRavvvzXRFoRIwAlRPyTgRGZqI+CM/t1aR1NGthdCNFN473V4nzaJe78QEnBC1A8JOJGZugg4IYTwkYATon5IwInMSMAJIbIiASdE/ZCAE5mRgBNCZEUCToj6IQEnMvPO26cSbkIIURsQcN27d0m4CyFqhwScyAwN8M5ntybchRCiGNOmPRCtXr0k4S6EqD0ScCIzWzZvdCJu+fJHEseEECKN0aNHRh9dfT9q3/6WxDEhRO2RgBP15okn1jkhd+HCueiN00eFECLBm+ded+3Eq6/ui4YOHZhoR4QQdUMCTjQIDIfs3/989PrxQ0IIkeDo0f3RvfeOT7QdQohsSMAJIYQQQjQxJOCEEEIIIZoYEnBCCCGEEE0MCTghhBBCiCaGBJwQQgghRBNDAk4IIYQQookhASdEhXHyxBHH68cPRk8++WjUpk3rhJ/68NRTm/P2r1x5N+GnEF263BZ17lwV77dv3y7hpzGwMjK2bXvMuc+bNzM6e+a1aPeu7VGHDuVJmxBCNAYScEJUGEx2+vZbJ6NLl95y2+fPn034qQ+hgLt69b2En0K8++7p6NzZ49fC2pEfVmNx7uxrMZQR84xt3LjKbX/yyQX3f/Him4nzhBCiuSABJ0SFgfiwbVuujO0JE8a5bfjs04vOrVWrm93+1z+/HE2ZMtG5dejQPjpyZG983tSpk9xx9t97741UAff5Zx/EYZvl6vLlt2O3TZvWxGmDXTu3uRn1LW7CJX2ky9I3ceI9Li3sEwf/Jq6AOMO8Z+Hy5Xfcv5/O1q1buXSZn/nzZ7v9T79IHyLUysQskJZGH/wsWjQ3EacQQpQbCTghKgyEAxald94+5baPv3awWqjd5ATGqFHDo379+jjBhNvcuTOi3r17Ru3atY3eOH3MnY9oQhwtWjTH+fnk4/Pu3F69ujvhkibgHn10ddS2bZtoxYpF0fZtjzt3xMvs2dNjETZmzJ1JC5wXFuFcvZJLI+kjDXYu5yEw2a6q6hiNGD7Ebdu5pBPhWAzyGJbVHXcMi9auXea2CW/JkgXxsc8+vRRvI+A4jvB84YWn3TYij3xbOkh/z57d3RAxaR80qL9LP2kP4xVCiHIjASdEheELG945Q0hNnDg+z717967Rhg2rnIDBneHCjz563x1DdJjV6N57c1a7Bx+83+0jgtIEnL+PlYp/hI65ETbvmhUTcH48pI99E3CWHt8qhrC0bd6lY5HzYgwdOjj2D926dc4rE7aXLSsu4BCPEybcHZ+H+PUFnPl/791T8TZD2X68QghRCUjACVFh+KIEEDpYx3zxs3z5IieWsM4hknBLE3B2nokxLGppAg4LmO1fvHjO/V+4kPsH0kQYCDjezzN3Pyw/HtJXFwGXBT5U8MsKq9mxo/vd9siRQ/OOScAJIZobEnBCVBgIioUL50SLF8+LTp44HAuMs2eOR088sc6JI94lYwiQl/jvvnt0tPPZrU7AjBo1Ik/AAUOxuSHVuU6kpAk4rGsIQgTQ+vUr43Ts2/dc9NBDU9w2Q4ocJ6wxY0Y5P1i8SBOWPtLHMfZJH4KyVAKOL3N5z478mxvDzoTPMOkHH7yVeAdOAk4I0ZyQgBOiCcGQarduXfLc+vbtnfCXBta40M2HcMNpQdjnnbvQbzFIY+jWmKS9KyeEEM0NCbgyMGXK5OrOsmu8f/vtfaJHHnkkz8+MGTOc244d26Pf/va3iTCy8stf/jL68Y9/nHCHv/7rv47Gjx+fcG9I9u3b16D5EUIIIVoiEnBl4MyZM9EPf/jDeP/f//3fI35Tpz4Yu/H76U9/Gp0/f95th2HUlr/7u7+LRo++My/c//zP/0z4g3/8x3+Mxo0bl3BvSP7kT/6kXvkRQgghhARcWcC69m//9m/xvv0uX76c5/bSS3tiAcc2ou+jjz5yx7HaHTx4MC/cY8eORVVVneJ9zv2P//iP6Bvf+IbbHjZsqAsLAffhh5ejn/zkJ3mz/OOnR4/ubnvRooXRt771reiNN96IbrklfUjqu9/9bvT9738/2r59W14Y3bp1c+du2ZJ712rbtq3O37Rp0xIC7vXXj0d//ud/7vyb286dO517p04d3bG77hqdiFsIIYRoyUjAlQEmX+XXuXPuXSF+3/zmN6N/+Zd/if3wGzhwQCzg/N/cuXOdqOKHkMM/ljN+fjzhD1EW/hhS9f1PmHBPdODAgTw/froA0cdwq/8jTxbG9773Pff/zju5CVb93z//8z+7f9xHjBied2zfvr3O/W//9m+j//qv/4rjmDNndl78QgghREtHAq5M8Lt48WJ03333Rn/zN7+L3V599dXo4YcfdiIIt3AIlffn/umf/in2//d///duG8HzP//zP4l40oZQ/fD4bdiwPt5GwBE+vz/90z91AnHy5AfywvRFGDz99FPRX/5lTgjy+4d/+If4GMLU8sJXk//93//t/HTseKv7f/HFF917fsDvlVdecQKO39WrV/PiFUIIIUQOCbgywe93v/tddOnSJSdazA2rE5YrRBFuoYBjiNMEnL0717fv7e7/O9/5TiKeNAHnvwPHb+PGjfE2Ao4hTxNaiEKEph+mHbP9rl27OAFpYZw+fW3m+r/6q7+KnnzyyXj/V7/6lfPDcGra72c/+1ks4ELhKIQQQogcEnBlAiuV/czNH5Y0t2ICbtSoUe4Y79Mh1MI44Pe//70bOrV9fjUJOCxsP/jB953biROv58UP4XtspBFhZmH47+bxHpxZCf0hU/YZmp0/f17sl+HcvXtfjgWcLzyFEEIIcQ0JuDLBxwi+mIEjR44k3IoJODAh6H9I4PP55585wYZlC2HFryYBZ5a9X/ziF84vPz/M6dOnO8scHxgAP4Z2LQxfwPGeHz8EJlY6PzzEGuHwtS2Ckd+KFcsl4IQQQogakIArE+vXr3MixcQMDBjQP+F2/vz7efsIOKb7sP3Dhw+547xfFsYBfBGKSOL385//zG2HAo602DYCjrngsOrZD4tbGO7EiffFx8Pw+AjC92uCECudvetmx2w4lt/zzz/v3EzA3XHHyES8QgghhJCAa/L867/+a54gEkIIIUTzRwKuCbNkyRIn3vhQIDwmhBBCiOaLBJwQQgghRBNDAk4IIYQQookhASeEEEII0cSQgBNCCCGEaGJIwAkhhBBCNDEk4EQmWrW6KeEmWg7N9fqTr6x5K3ReIfdyUolpCilFGtPCrM81F6KcSMCVgb59e0cnTxyJWbt2mXPv06dXnjuMHl18Mlsant69eybcoUOH9gm3huLKlXcTbrWFNBeaeLgYvXp1T81T585VCbeGpmfP7gm3hiAtP41Nt26do02b1iTci/GNr1+uPq9Lwt2ob3lxrUO3xuDq1feiN04fc9uF6lshPvv0UsJt8+YN0dc/v5xwrw9Lly6Ihg4dmHCvLaST6xe6VxLkkTTWJ59p2LX1OXf2tejdd08n3NOg7Trx+uHo/Pmz0auv7ovdZ8yYHLfZrx8/FG3f9nh8bPLkidH5989Ehw+/nAhPiPogAVcGEGXTpl5bqJ2b/vPPPnDuNFr+sRA625kzp7pthNtjj61z59CAmJ927dpGDz00JTp2dL/7N/cBA/pG8+fPdsfZHzZsiGuQpk6dFK1atSQRlw/nLVw4J6qq6uj26egGDeoXrV69JJo48Z7YH2IKv34HPmnSfe5/9uzpcZoRDJbmNm1auzTcccewvDhpvMeMyS2nRT7oCMmTL9gI+4MP3nLHBw3qH02YcLeLY8WKRVH37l2dn5UrF8dlZsyfN6taOC/PcyMN69evzHODUaOGu7j9siQNK1Y8UlCoELYJa8qYc7m+a9YsdW6FrlG/fn2ixYvnxQKXMiD+RYvmRHPnzoj9kU8eBLhuvvWA+kF+/bQQF9fErl0aPXp0i7c5f9q0yXnHcSMM26fOWfkOGTIwWrduRXwsrbyqqjq5umLnGDy82DUGKxc7n3LmH3fqm/kjvjFjRsX7fnn49ZHyswckoHwoB8qTcmvfvp0rVxNqhMN9QXxp9c1n0qQJ7nrZ/r33jvOO3efK7MknH80TcOH1NazehukHyp26Zvvkwc636+1fL+LAjXj8cOzee+KJde7e41zSSdzcm/ihjeBcayMAP5QZ94yVGW1BKG7tOllbwjUn7Vx33x/X268vfpr9fBEe+6TP/FF/zE+hOmVYvSet5sa1te1582a64+fOHs8TcKTvwQfvT4RHXQiFL/ujRo1w19k/tm3bY649evutk7GwJ1yEaRiuEFmRgCsDoYA7Wt0w1EbALV++yB0Hnhr9fb+ToKFm33ffsGFV7Pfy5bed28GDe6q334ndrYO8++7RefHSOZA+/HzyyQXnhoCz8P2G67NPL7p9/q0jYpuG7J23TyXSTENNesxt69ZNrpPAv7k9tWNzHA9MmTIxju/qlWvp2LVzm0vX8dcOOjc6VOKztNs5uNs51rmQDnMjDX7+edK2Y+xPmDAuL5++31atbnZpwC/xklY6Oks70FGlXSPEo/nxr9GlS2/Ffq3T9MufjgU3K1vipX7gxrWztHDt0gQnHSLnsr3z2SfjvNlxv+6YZYxt6zgtTjot9pPldbcLk30rL7vGYVxhuVB+bB85sjf2x7ZdU4szrT4SB9vEQR3CjQ770y/SQseNJZlt/rl2lDvlSTosz359MyhLiw/hj9tHH73v/gnXzmW72PU1SD/l4acfqGt2jt1PiC2/7MMy9MvBF73hvUdZ2Lncm3aupc+uNX4pM8sPZcV2aIXHjWtj+bWw2TeB+frx3L2JG3EgBq1czI00k0fcyCfuVu/37XvOXde0OhXCMasnVu/tXmG0w/LKA6AJuGNHD8TuWNLC/O3Z82yeGyKZB5hQwC1btsAJX/Jr5SFEQyMBVwYQajTWCDfINRYHYwFHI0JHCv55mO1pwGgcrLEYP35MXsPhQ6PCP0/V+GH4gIaPxhh3xAFxYWGYOHG8SxPu+PWfwM+eOe4aTRpTO5cOBwsInT+NYpcutzk/CCosMO+994ZrPK0TpZG08EizDQ3T2dPA8XT61FObnV/O5598vvDC0y4u/OLP8uRDp2Lb1pHzdM4+ZcY/YWGNcWm8ShpHxGLL0kEacOP8oUMH58VBXvi388knVgPy6VvAsObwxE352XCcCTg6RNJlcYLlh7CsHrBtQplrhDtxIPywHFk+rfw5Tvlb/eB4Lg8D3bXDL+dz7UKrI/gCzvI5a9ZUd77VHYZLqTt+HaE+4I5lBjd/CDFRXtVUdero0oO7XWO2ucZhmvxOD3+ER96Jz+Lh3rHttPpIHFx3O84/ogWhb/USwWFlSL00AWdpsOuDnw4d2jnsXKyWXH/uIfyYgOPY009vcXWAbcIpdH0Nq7eJ+8nV1eHuHLuffAFnabXrhT/iQBixHQ4Z+vceZUGcZjnkWuOfMqau+NeaMuM6WZk9+ujq+PoZdp3Ig6Ud4cR1t+uEH+7ztm3bOOGFtc7KhTRTLqTBF3D8m1WPdBWqU5S3XSMrU2u3rN5beSHO5s59yMVJ+Ai4JUty7SoPLIivtPwhqH03wwQcQ6d79+52eSMdJjTff/9MtGXzxsR5QtQHCbgy4FvgZs2aHg/b1WSBs86cTsMal9oIuJ3PbnV+ECfs01nzlEt4vn/8DBhw7YndQICYxcHvMO342LF3uc4TEWhDD9bwmvXGH77zOxF7ur9w4VwM7jSgvDfCufYkXFsB579rQh5JO2VGZ04a08qLsP00+O+wgAkSO9/PJ3k0fwwDU0YXL77p8kWnbgIuDAvCa+SnAWFN+hGL5t+3OJkb51H+Vj/sWpFf4sZqgBvpsuFsH1/A0Ymb1QKBYuny46Lu8E/eGf5m2wSIvRcXlhd5sHxZ/SYtlHuh6+HHuWjRXLdNfDwMEJ9ZgnBPq49sU4cIy+qQP5TMuQh522aYvZCAI1/UWSANDMta/AxJ4scEnNVh2L//eRdOoetr/vx6699PYdlT13wBZ69f+NfLjyOMJ03A2THO9dsI/1pTZogRKzOzUNq5lj67TqTdv+Z23f2y8c8L0+wLOMQX21wXrNGF6hR1l7zZMKnVe/xavfevrcXPNUfA0UaE7UCYThOSpIHhaO4prnFogUuDtB2o9hu6C5EVCbgyEA6h0gDTyBYTcPYEz/AeT47WWBQTcCZCeIkWPzYURCNGp02Hz9O9+cdP+G4OzJ+fs7DwTkcxAcdTtVlTrANA6PAfCjgbpsVqZFYyGDF8iMPen9q9e0ecPxrXUFiBWQosXdYRWpmxTZkhaOh8/cbb8h+mwX8nzOLmnzyy7efTf1+G4VsrI4anaxJw4TXin30sMPzXRcBZ/TA38su1o2PDjXTZ0KqPCThnTfviWlP+vIBv6fLjMosVnStWQRNLuJmAC8vr+eeeivcpD7vGWE24xuH7j4UEHPFZeugMbTutPhIH+2YJI466Cri0+mZx8M/Qow3h+RY4LDtsEx7hFLq+RpqAs7IzP5xPXTMBR1lTlhzzr5fFAZY/o5iA4zy/jfCvdV0FHGm3aw7ESVrJj1n3R44cGj/o+eVC+L6As/O4b3h3tlCd8tMCVu9pt6ze27X173fiQcARHuGaBY/2ww+P+9lGIAzuzWIC7pOPzzuLpe378QpRXyTgygCNmb03A6+8krOcmIDzj/nYu100CDu2PxF3nLt3bXfu9oK84X9tRmNkVpKzZ3KNGeKATgM34D0w3Nn2h1DHj7vLudG4vfXmCeeW1mGy/cbpo84v1gkaaAuvY8cOeWkjLEsbHQ/bNI7PPPOkswIcOvSSc6NT5EkXfzY8Gb6bYpYQnrZJ1yHPsuiXGf+k0zp08Ds4cyMNfvhAB8AxXpTnfD+fvj/Kig6dvPDUz38xAedfI97LsXcSraF3As4r62ICzvIK1A/+SQ+dCNtcO+Kg7A8ceDE+n/I2Cxx1CL/kwcS8CWHwxQ+dKx0u21Y3LG9+edHB8uUe+5bu8BpbWgyuNce41vzbi+jEZ6KItBarjzYcDlaHEC1WF3EPBRzWPevkC9U3MOswllYT8JYPRLDVbwSnibC062v49da/n/y6avcTw6VmgTPrm3+98t9r7Z8XTzEBZ+nAjTbCv9aUmbUDlBmvA4Tnsu9/MGDXHPhgADfaNkuzvU8WlgtpJo/sWz4pY384OK1OhVi9p/yJAze7tpZeS4e9A8c9a+/WYd0Mw6Q+27UF6hXDtIUEHOVmYhjMwunff0JkRQKuiRE+udcV/0nVhlBxM/FWCN5l8UVdMbKkEctYbdyywHsyoRtpDNNJfOFTdzEKTaPhvxOXhTRrQm0hT2n59T9eOHnicLRx47Vh3xDKIMwDHWChdHEszXIbYp2xT6EpcGqiNvEB1zRrHDWRVs4GZVVoKpRC5VgIrmmhugZp1wsLcl3jMUzsNAQImPC6I2LMOuYTWr1rQxh2SLF2izK1d2VDTGgVgjI3K2ttyF2j4mEKUVck4Fow4TtwomWApbKhxLEQQojyIAHXgsnyxCuEEEKI8iMBJ4QQQgjRxJCAE0IIIYRoYkjACSGEEEI0MSTgRILwi7amSF3ygN80/2lupaCx46lrfGn+C5VZbch6XlOhueevoalPXbLzQzchWgIScGWAeZiYy4oJaJlPirmmCk05UGr8VQwM5iuyCTlrgz/ZKPMppYXZ2DBPm23XlB5/EtVCYZQKpjJImz+qFPgTzdY0/YIRLsYO1F0mfLWJcmsLy3wxBxhzkYXHGgKb7JW54cijP+kqX97aerwcY+JbZvVnm2WO+C80aW9d8RdGB3/1ENIXLpHHWprEz3qhTCbrzxMIHEubNoV56ixfnGPThtRU3yuNcDF54FqmrbqSRriubEOyYsUjrvyt3viT8gpRbiTgykC4EgMNA/NyMTcSE38ywSbuTFZp8z8xwaRNwMlkkrY0DPMLcY418MznZseYzTyMm2VpbF4s/DFxKZ2ZP+GnCTjSw+Sd/vm4kT7mVgLOpbHln9UkaEwJ0w/Pz5OlkUlCWduSp2fmU6IzKjZvFes8snwR65VSLrZSA1i5gOXdltPx82h+yAMT11p8JuBY2N4WDPfDIG5Lr83SDpQ95xA2+HNbscJD2jm5ZZiW56084KeNMmUyVH/lCuaqIp5QdJE+JlVlclRb4gf8hwGunwmtNAEX1h+DciOcUMCF1xs3Jq61iYDB5hRk5n4/TOpGbrHyXN3gOGGsWJE7N61s8MP1ZQkpypc1asMJqwERRCdr5cZKH74wfvHFZ+JtJmo9dfJVd9wm+EWsh0KAekbe+Dc3ysVWrvC/4uYaU8epU6EYCQWcredpbjaRd10FHMLNpoPhvli6NLeAenhPcz2YpNjOoz5TP2kfOC8Ml/ntqBNWr0lrLrx+cT3jupFff35B/Ni8an7cpImJeP06AkzuSxihgOMeoI4xAa5/b4TtiBFeN2vjWBbQ0m/HbJ5Hyo0lDK2u4Yd4/TwCbYNN/ssDSPgwI0Q5kYArA3RILJ7OE7+tkThnzox4IWl/0XB/QWmgwWH5FltYOlwE2mZX9xePh3CRdXNPe1rnfDoRhAEzhrNP58k/cZA+37pRyAJn54R5Io3FFhTn32ZuBzobW5TclrB6791T7t/WWrWG2I77M66H6TnzxjHX0Vg58E8eSIMtJO6HwXx5NPaWXltQHguOzZRv69ka5uafc7Q6POKhk6czwN3yzTm28Dziqtii3YatfUpnytqMVr50dNZpsZ4jM9azHQo4OrOw/thxFmO3uhR2Wv4+s9iTBx4qWOYIN86j/tki6UZonSVfiBuuRVrZmB9EAnmkTiIM0kRSWN9DsLKxisX27Y9Xx/OB22blCrZt1YzwHMrOro/ljW3OM6sZbqzUQNopR5YFC9MWCjjKmvSwb/UX6irgbGmrcKF0v4xZ2YDy5P7BL+LN6o2te2wLwAPHyAv1jXSyGgHljX/qMOlw9bK6DeK+ZP1Q0mv1nBUUCNPuQ6tDhE2ZsfoG7mmLyft58y1whdoRwwRc2MaRJvaPHNnnjtvKCrS9hM/Euk89tdk9FJBH8mF59MM3CNOumxCVgARcGUDA0VjQuNB5MByFBc5f1skaGGvMERO54Zec4JswYZz7Z/kdltJCTHDcGsxZs5LWN8KjYa2NgDt29IDbpiFnn/Txb2ncu3eXC8/Sauf6nbSdY8csT6TRLEAct3Ul2cZSQPpsTUagcWXJKpbcMmsSS0Xxb8s3US65Bj4nGtIEXJgHg/hMvEyaNCFexsgXcOaXPNhyVnac+MNFqm09VP+c3FqkubRYp+gLuJdf3uW26djo8GwhccqUa4zVyE+7dcR+PJRvbQUc4iGsP5YfOx+hXEzAAeHs3bvbiTD2ub5p9S9NwNl2WtmEfky0A+n1w/bXi8W6gpUNzEJlli/IlU+u7lLPTp961Qm5cGJrrHnkjXMtb6QLEcQ2rz/w76ed6xmKkVDA2TJn7FOv8c9+XQUcUO8RMPixeyO8pxFAtmwZVrqw3rANXFcebqyeYNFkiSv/egAiesuWnGDkfuWYiclCAs7ONfEV3h9hmfkCrlA7Yvu+BS5s4xCv1AOupdVb6gB5pN7jl7zYg4OfBh+ssFwXW85MiEpAAq4MhEOo4C8kDdZo02ggUmhAGNbE3beUhItAW4PpD8FBuMi6uYdPsxauWXtYVsfi4d9f7No6lVDAWZiFFkInjcUWFA/T4y9K7i98TrkQt5WL33H5As7Sg5UFf7Z2pQ2n0NiHC4n7YRQScGzTOZh1xscXFHYO1hAEFW5Ylaw8rIxs4Xk6SK5VTYt2p3XEVr5YO3DDimLiIxRw/sLnVn/seLgYu8Vh+bFt1ptE7HE9rbz86+tTTMCllU3op5iAw2K7ZUv+O1PUCxNwJih8KGOzQmGhQsT5x1kP1CxLlje2LW8IHP5J+8WLubTzwBOKkVDA8Y8FirLg+lgcCDizQPvnU8b+fiF3W1/Vv6cJa+vWTe6hgG3y6dbX9eqnD0PK+KNsEPfcc6GAox5aeTIsavcU/5s2rXXutn5sIQGXtpi8nw7qmL2TWKgdsX0TcIXaOOoQQtnuceL247dlx9Le+yX/ZmUMjwlRbiTgykCagKNBoqGZP2+WM+vTGOFui4KzbQ0p74KxT2NFw0TjTINMB1hIwBEOT55mwbPOOme1mxWLFsg14BddI02Y1mDiRhpJn9+JIwwYbkV4mKWQ8OycME81CTj+GQ6x8PGPSOKdL4Y/rNPFHx2ClYvfyPsCzvLo54EyszxkEXA07Dyxcy3T1o5ME3AMc5FG3suxxdB9AUc6GAqkM7QOA4sI28TDe1r+e3Am4HiXiRfjbYiI4UbELe/zcZwXsXEPBRxxh/XHjlOuvJtF3MUEHOVJmZE2RAzvzWURcGllE/opJuAQQQiw06eOOlHxxBPro4+rBY519GkCjrxduvSms8gRj28VAsqAvBGu5Y10hQLOLGrc07iFYiRNwNk7ek/t2Oz23fnVAo58IOqoW+SD62hCJnzXi/rJEDZ1m/rLsLfFYfc04fIAYIvFU2eKCTjyST2gvpmlMRRwFscTT6xzaaVeWV2iHKlvVkcKCTjKDKs1ZYbfsMwIk3uBdqVQO2JYuRRq4yg/9u1B0EQqQ8CUP2koJOCsLpJerItg4RVaY1WIxkICrgzQONIwhe40fjQMfifBE63fANKJ2cvTCAdrmHmiREBZg8ki0n7YvMxOI2jv+1hDSqOLfxpi88s+4dCI0riaAMBiZWnEOmH+EVu48dRNGgjTwuOcME+EbenzG1q2TcD578AxLGJhmAgx/7YoO+Xil5Mv4CyPlh72yRedI27kCWsD27UVcBa/QUPvL27tCw//HGv8XYdU3cla3jjGNcKq4qcNawdDoPjxw7R00RFbGuzjAaCDxc3yBaGAYzusP7iRLtJAOSFuigm4detWuPPp+Pknn/719UHcFBJwEJZN6McEE4QCDhBt+EdwffZpznJtx8yK6TN9+gOunlJWVz58N5owYWzeccIB6iT/W7c+5tJnefPTg2WYY4iIUIykCTjAv71vyTYCjm37Mhkoaxsu9K1KQDrMH5g//55G7LDNe6YmNF29CcreQCD5YdJepAk4BDtuxGXDijxgWftgw8uFBBxYHIjQsMzsfTY7N60dMez9x0JtHMLbt7gBr2NY/tgnj3w85PsBvywMCThRKUjACZEBRJVZ3h59dLVr0GfMmJzwV0qKWVKEyAL12B4uEUS++GqqYJEr9A6hEE0ZCTghMmAfkWBRAb7WDf2UGgm42vO1r91YlNB/S+XixTddvcbyB2nvyDYlsEqGQ+NCNBck4IQQzY5QoGUhDFMIISoJCTghRLMhFGH1JQxfCCEqBQk4IUSzIBRfDUUYjxBCVAIScEKIJk0ouHwGDRoYjR59RzR48MDo9r69o959ekZdu3V29OzV3e33H9A3GjVqZDRkyCD3zlQYhsScEKISkYATtcZm529ICoVZk3uh402NQtM5iNoRCiyjqupWJ8j69bs96ta9S9Sla+cYE3A+uPft2ycaOnRI1LnzbdHNN6cLuTD+LPUwyzlNkSz5bG73dxr1zRvnt4RyEjUjAVcGWDeSLxf5wou5mphwlXmk2OcLMOYCY9tmZQ8/5be5t2yftf6Ys4x5vpgJHjfmbmOeKsJhDij/fCbDDdNUCJvDyuaCSpvssj4QJmsR+m7Mk4c7+Q/92zxa4aSm5cRds+rrSFkzDxVlxXxn9hUf19OuV7jMERPw8h9+7RdeI65tWA/8OJgHj7KZMiU3/5XND8d2uMyXPxdZKSg0KWpdIZ3h6gpGKKyMdu3auFUoevXqUS3KbssTboXEmy/ijNtv7xV1cvfSTXnhb9v6mJsg2NLB/HFh2mqC6xLOZdZQcO+k3TflIEt7wTnMUcg/ExmHx5sDYRtQVzjf5s3TF7YtGwm4MsBkpzb5JWte+pOtIgBYE9P20wScPzEtSwXZpLksw8Ss/GyHE8nakkJgM6zz9MbkszT6a9YszYuDSSqZFR9hyL8v4PDLpJ2+fyZ8Xbx4Xp4bsEYi4bOmZC7uwXlxEbbN0USZcCwUcEzEyXI6DG+lCbgxY+50otaWyAph5Qr8+G7495cuM0gvAnvVqiV5T7eULXlMm7yTRtRfc5P1JmlkbXklsEll/cabJcEsDTUJOMqDMPzrGMaBH5swmFnyw3oDzD5PfaDcmeeLuoAb9cbKiEl0w86TucHsGhpW7mEcJuC4fmE9QVz5q34AaQivD+VMWR49us+tLGHupOvhh2cmhBuMH3+3mxSaOHLiLbuA41zSxES7N954QxzH4cMvVQu4C9G4saNdej68/I6bQPmRRx5297Kls6qqk5vE1l85wzABR9nZcm5GWr0MrwflxQTPdq8D9w71qb4Cjglw+ffDTru3eehcu3Z5nltYH6y9SKsHQB7IG//+OYUEHOXsr/MLlB9u/jxv1GfKl8mA/WvC+bbahblxX3Od7L7GD2VAO2BlQHnb6jchadeGuBctmpMXN9tMTm4Pd34YxNW7d8/qh+5FcX0hT+GygpxPWn0Bd++9ubWfgdU9wrpDvpiguFQPDKK8SMCVAcQByxeF7pAm4Ojc/Y7A3MJzaUhokNj2O3rrMGzfFm22xsREIo2G+aFBoaPHnX8TcP5qEeZ3/fqVsV8WYPfT5PtnOSMLz57MrcG2bbBZ3mn4EQ4WNmsS2ioAvoCzMPn3G00aarOK+em12enBLyc/vYTnr+Zg/okXQemfg19En+8WiivCIj1+480qErZ6Q3g9QwFHWCwF5HcWYRykz5ZmKiTgCNfyR3kyl5ytYsEs/KzGYGXJmqScYysD4G7W3GNHD8RurMCR31m1z7vuVq8419xMaJrV2U875OpfbiWEyZNzooJ6lptp/0J12rrliTeEFu+6Id669+iWKuBCwRYSCjjuJVsdwUTcxx+/X/3AlLOQ485M/uyz8sOHH+YWfqdDtjrHv4Vh4H7q5KvuH5GKmMY9rV6mXQ+uGfHixr6tDADcO/URcKSXvNkqE2n3tq0egrsvGMyNe4R6zX5aPTDwC1iXLW/4KyTgcLP0pcVp6WOVDluJhIddrgmWbkuHnc8DiblZe4JAsmtnIxgWr18/odC1IW7cLW7czYLOw56tHGFQRnbtEWTk29JvfmxNauB8E3C2uopfd9im/lDvLH085FsaRfNBAq5MYGmiA+XmYoFpc08TcDxV02DTcNK40dGydqQfHuKIG5snY3PjyZYlbczaZ/gCzl+IPW1oLRxCNf+7dm5z/9YI7t613RGGUdNSSJxLngiXfOLGmo642xCypYGO0Ro1a3BZ/9PiBl8gWZpZNsfWQaTj9NPor68Jfno5F2sRQsuW+9q7d5drFG3ZImDYmtneSRNhE5cJY1uvlmWM8Ounz18WrJiAI19YMBAUhImlxcIiLVx33x0KCTjw82/rqfrHuaZ79+6Ojn4hYNOGafxrTp0Ny92vJ358dFDWKWLt4J/ysevjkxtCzdXzDRty4s0EG2V388254U3EFdaK7j26Rp2rhRxcE3C1F3GhgDNuv7139b3U2sVDeq5ceS9Ohz+ciohjuS1EJ3WdsjGhFpadLU83a9Z0t1+sXobXg2uGhY9typnz/XvHF3DWxhQjTBtpsnj96+ynD6sObY7dL6Q/fB+LcwvVA+ChkjhyltZc3jinkIDj+vOqie1zLqMXpM1EW85frr5yj3J/8LDAcc6fNGlCXvr8+xr/4dJfbFM/aUP8tPhpCK+NHbO42bb7A0sh7n4YlGEo6hDlhw695Kz57Pv3IOeHAo68W/mTV+oP/Qjpx1jgP2CJ5oMEXJmhE/PXOCwk4Pin0UQoYG0IBRzQAL1x+li8z00cNhbgCzj/HZWwgYVQwJl/EyS2cDQLYBv+kJcviPxOPhRwPCHbMIitrWoCzoZGGQ6wxtUEHB2UH/fJE4fjOIDhEHsK9f3bcVsL0UgTcOTRBBudCu6hpdE/h3fOQuuYYWVAB+x3FOHanrbWLI2yWVsMs4L5cdAB+ENRdRFw/moOCAuOv18t/M0CmVaHcCtU7n69op5YfCY07RUALHtcH1swHEtemE4TcM9UC5NQwPXv38eJqk6dOlQ/pAyLxVtSwNVOxKWJN8IaNHigSyuCkfR8+OG7cTouf3DNEuwLON6N88vHzxd5NeGAVYT9QvUy7Xr4IsEsS/694ws4RD/tRzHCtJm1vtC9zYMC7qzaYAKC9PvhWFhp9cCgDuHGv+WNcwoJOB7W8M+9z3rQpM9Pm6XPr88moigHzidcO59t/77mng4FnNVPwgnrZ03XxhdwiF1zD9/fpc05fPjleJ8wsJhRpvZw79+DnB8KOL/ukBZbw5Z1ZskPAtCGx0XzQQKuDPCuA0+sNBY8MftP24UEnD9MgrsJON47wY33HxBbiDOGL2h0aDRnzZrq8M3ndRFwdLoMYxYScHQcNA485U2cOD7RWdVWwBEODS8i1V76pyPCkkG6iJewrCGzRpD8s4A41rn582blNZTWGGO9wqI1YEA/V064TZv6gHMLh3XSBJxLW7U74RM/QxT+OfijoeR9GLa5pjUJOPz7T9WcR2PMdbYF5nGnbvgC39LP+qthHKTN3qkqJuBw5zjWsFDAUf/Gjr3LDcURNsPF1DXqDO5cCxtStXInXr/cCwk4zqEuss82nRP/WGlsaNdPJ2VOGgYO7Oc62o8+ei+aO2dGtUja5KwTiDcYMWJY1Kt3z2qxVfUFxYTcbdGGjeujlatW5Im6ULiZeMtR5b5m7dKlc7RgwWw3VErdZJjqg0u+gLvk0kbZsU2+2A7vK/JJHUcA0FFDoXqZdj18kUDdJHyuIXETbn2GUEmDCbhC9zbXnfQioIgbMco+72hi4XL5mDbZ/afVA4N7m7zhz/LGdpqA40EXYYKllmO8f0f6eBggfeTf0pcm4DiH8xkStvMpW+oYIxrWroQCzuonbQjbtCF2rKZr4ws4zuXBjjyHD0S+gGO0BL+0udz3hIf1jPLjfOoH54cCjnNoD+w+ov6YZY8HYMqehyz+9wcfNYmmiwRcGcCqwk3IjQa+uEoTcDSQPFlyjgkX3wJnwyiAeLNFqH38l5Ax+fNPR4tgMvewgc2l55I73wSc+TcBB/77GbwX4p9fWwHHtr0n4lvgGJ5kyI282xM3fv0hB8oM//jx32mj06ej55g/XEkjaOn10wppAs7c2ccyFr4QTMdg4b3yyh4XbyiuDCuD8+fP5pUHQtTyz9Mz18uGTDdtWpsXBp0WjXsYB/k3y91jjxUWcHTAll4n4Lw82xfOhMu/3wGBDenwNG/ljmXDL3e/XvkdN3UX/7w+QGePu399SLOfzjlzpjtxc+zoK87axXuGvIOG344dO8QCbuDA/lG37gyfmoDLF3GhkEPA7T+wLyHYksItJ96A8HtX13OsnMdfO+DKrE+fnq4ekjbSawIOP1hrsMThj5fP/Xzhb/XqJS7fCC7ED+5p9TLtevgiARAXlAn1JrTA1RXioGxtP+3etvaF+MCG/02Y83BiYaXVAz8uYPjb8sZ/moDzw0fsmMXRHna4nyx9fn02EUWd43zqrJ3Pw45/X+M/FHBWPyGsnzVdG1/AMfSLH+49Kx+DNPgfstn7bPbOqLVBlg7ODwUcdcfKhyFl3LhO1C/Swf3GtcRPOdZtFqVBAk6IRsaGxP33FUVh/I8VDBNviOkePbtFt3XuVE1VgnxRl2P9hvXRvv37EiLPF2yGHxZWOMS5/1WqT5huIYQoJRJwQjQyWGARHuFL3yKdUCj5Ag7rNUO6OQEXki7o1m9Y94WAS4q7ULTlyIXH3HL2MUOaiAvTLYQQpUQCTghR0YRCyQTcDTd8Nerb93ZnOau6rVMexcScCbikUEuKNoMwu1YLxVtuaVsdrwScEKL8SMAJISqaUCiZBQwBx/tvVbd1LEK+sIN16xFwexPuhbkWXvv27V28aVa4MN1CCFFKJOCEEBVLKN5865sJuE5VHQuSFHQdqwXc2mjvvkICLunfhy8CCwk4iTghRGMiASeEqFhCgRQKOBagZ4izU9WtBcgJuV//5tfRc8/vjvr17xutXZcTcLhPuPee6Ec/+lF015jRCfF3jVxYzBXXvv0tEnBCiIpAAk4IUbGEAskfPoVevXo6y1nHTrfmEQo5RJuJOBNwE+4d78TbhQvnE2ItDaYYyb0DJwEnhCg/EnBCiIolFEihgGNyXT5iCAVcGibitu/Y7gQc4u18tXgbfdedsUgLz/Hhi9c2bVpJwAkhKgIJOCFExRIKpFDAfe1rN7g1UEOxVYxf//rXET+EW3isGLxvZ/FKwAkhyo0EnBCiYgkFUijgvvrV66MePbu7Dww6durgkRRgxtp1a6Kly5Ym3IuB9Y2VIL76VQk4IURlIAEnhKhYQoGUKuB6IK76RLd29AVcw8FKD/369XVLZMkCJ4SoFCTghBAVSyiQrgm4nIjDItamTRs3vImAKwUDBw2Iunbt7OKXgBNCVAoScEKIiiUUSPki7poV7rbbOjmRFYqvhoAvXVu3vtnFIwEnhKgUJOCEEBVNKJKSVrjro+uvvz66/fbeUf/+t7ulsEIRlgXeexs8eKCLhzhMwFn8oYAL0y2EEKVEAk4IUdEUEnBmhTNx1arVze5dOERcKMaygHirquoYhx9+wCABJ4QoJxJwQoiKJhRwvojz34XDCnf99de5/yFDBkUDBvRNiLKawOo2dOjgaiF4+xdhXZc6dBqKNwk4IURjIwEnhKhoQqHkCzizwvlDqYguJtzt3bunE3G1HVJlQuCBA/u59+mIw8SbWd4k4IQQlYQEnBCiogmFUijgQhGXE3I56xng3q1bl6hfv9udZW7kyOGO4cOHVgu2AU7oMfyK+Lvuumvn+e+91STeJOCEEI2NBJwQouIJxVIo4mwoNV/E5Qu5Ylx33Ve8fXvn7Zp4I/xiAi5MrxBClBoJOCFEkyAUTYWscIUscVjXEGpJkla3uog3CTghRDmQgBNCNBlC4RQKOd8S5wu52uKfGw6bpom3MH1CCNFYSMAJIZoMoYAKBVy6Na42Qi5fuKWJNwk4IUQlIQEnhGhShCKqtkKuNoTnp4k2iTchRCUgASeEaFKEQqqYgMuJuNoIueR5EnBCiEpGAk4I0eQIxVRthFy+oEu6h4ThSrwJISoJCTghRJMlFFYhoSirifD8kDB+IYQoF/8/z4h1s4kIqHcAAAAASUVORK5CYII=>

[image3]: <data:image/png;base64,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>
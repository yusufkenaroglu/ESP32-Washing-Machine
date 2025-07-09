# ESP32 Washing Machine (Modified Scale Models / Toys)

This project is a firmware and hardware exploration that transforms **toy** and **scale model** washing machines into realistic, fully automatic replicas powered by **Espressif microcontrollers** and **FreeRTOS**.

It aims to simulate real washing machine behavior, including:
- Real-time cycle control (fill, wash, rinse, spin)
- Auditory and visual cues/feedback
- Modular FreeRTOS task-based architecture
- Peripheral interaction with polling and interrupts

All of this is fit inside miniature machines using salvaged toy parts, in-house embedded code, and a love for how real ones behave.

---

## What’s Inside

- Arduino IDE FreeRTOS firmware 
- GPIO-based control of peripherals
- UI animations for different brands
- Modular code structure (UI, state machine, I/O)

> This is a hobby project intended to be open and educational.  
> It’s **not a product**. It's an invitation to tinker, learn, and contribute.

---

## Demo Videos (Click on the thumbnails to get redirected)

Here are some clips showing different stages of the project:

[![Modified LG Mini TurboWash DD Lunge Tumble Wash](https://img.youtube.com/vi/q8yADDqyYLE/hqdefault.jpg)](https://www.youtube.com/watch?v=q8yADDqyYLE)
> Showing one of the 6 wash patterns implemented

[![New 1300 RPM Direct Drive Motor (demo)](https://img.youtube.com/vi/g4-hl7QYWUM/hqdefault.jpg)](https://www.youtube.com/shorts/g4-hl7QYWUM)
> Testing the maximal unloaded speed of a FOC BLDC motor

[![Modified Bosch Toy Washing Machine – Pump Test](https://img.youtube.com/vi/PkpvrBWGcwY/hqdefault.jpg)](https://www.youtube.com/watch?v=PkpvrBWGcwY)
> An earlier Bosch build that uses a peristaltic pump to act as both fill and drain pumps

More videos and technical breakdowns are available on my YouTube channel:  
🔗 [@wishywasher1330](https://www.youtube.com/@wishywasher1330)

---

## 🚧 Open Source — Still in Progress

This is my first time releasing this project publicly.

The code, organization, and documentation are **not yet where I want them to be**.  
I'm publishing early to:
- Invite others into the development process
- Help those who asked for the code
- Share what I've learned, even if it's not perfect

You're welcome to explore, fork, improve, or ask questions.

---

## ⚙️ Hardware Overview (Not exhaustive)

| Component         | Description                                 |
|-------------------|---------------------------------------------|
| ESP32 / ESP32-S3  | Main microcontroller                        |
| L298N             | H-bridge for controlling pumps and lights   |
| ODrive (optional) | Direct-drive brushless motor driver (LG)    |
| TFT/AMOLED Display| For boot animations, UI, program selection  |
| Buttons/Switches  | User input for cycle selection              |

---

## 📜 Trademark Disclaimer

This project is an independent, non-commercial initiative that modifies **scale model** and/or **toy washing machines** for experimental and hobby purposes.

Brand names and logos such as **LG** and **BOSCH** may appear in:
- Display animations shown on attached screens
- Physical labels from reused or salvaged parts

All such trademarks are the property of their respective owners.  
They are used here **solely for visual realism** and **do not imply any endorsement, affiliation, or sponsorship**.

> This firmware is **not intended for use in real washing machines** and is provided as an open-source platform for hobbyist experimentation.

---

## 📄 License

This project is released under the **Apache License 2.0**.  
See ['LICENSE'](./LICENSE) for details.

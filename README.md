# neuton_ble_pull
## fem_test
This branch is for testing a FEM with a simple-gpio interface and bypass mode. (Namely the [SKY661112S](https://www.skyworksinc.com/-/media/4C7B484628BA46BDADCB2E2AACDFA1ED.pdf))

> [!IMPORTANT]
> 1) This is purely a software sample. Please consult regulatory documentation (FCC, ISED, ETSI) for your area for guidance around transmitting at higher powers in this band. For instance, ETSI states a 10dBm max when advertising. The outputs were measured with a spectrum analyzer to confirm the approach.
> 2) **This is not tested for potential deadlock or timing issues!**
> 3) The sample only shows driving CPS/CHL. In theory, you would have to wrap this around entering/exiting bypass mode around radio activity.

The simple gpio driver only handles ctx/crx timing. It does _not_ handle the pins that put the device into bypass.

<img width="916" height="279" alt="image" src="https://github.com/user-attachments/assets/c672b9e6-1c8f-46f5-b21d-39d95a5708a6" />

<img width="864" height="295" alt="image" src="https://github.com/user-attachments/assets/2f85fa43-19d6-4462-a27e-10d9f4df26cc" />

So you need to control the SoC's TXPOWER register to make sure the combination of fem gain and the SoC's transmit power make sense.
This branch shows manually driving TXPOWER when you're in bypass mode, the other shows using HCI TXPWR. When using HCI TXPWR in SDK v3.1.x, you need to work with inverses to get the true desired gain.

So with the RADIO peripheral's TXPOWER register/table in mind, here is the example output using the HCI TXPWR.

<img width="403" height="500" alt="image" src="https://github.com/user-attachments/assets/34d66102-6add-4345-bf45-c3ecfa704473" />

### advertising
<img width="794" height="407" alt="image" src="https://github.com/user-attachments/assets/6473ba66-32a6-45bc-b21e-65f6673d6859" />

ADV outside bypass -> Ask for -21 -> TXPOWER=0x18, or +0 dBm. (Result=+21)
ADV in bypass       -> Ask for -13 -> TXPOWER=0x3F, or +8 dBm.  (Result=+8)

### connection
<img width="560" height="459" alt="image" src="https://github.com/user-attachments/assets/0ab467ac-62e9-4c57-860c-db0da031e8cf" />

### outputs
<img width="280" height="81" alt="image" src="https://github.com/user-attachments/assets/90b80b74-2217-4684-8213-ed7cc47a90d4" /> <img width="35%" alt="image" src="https://github.com/user-attachments/assets/2c3a3439-bbf2-46e8-96f9-c70d3d72b9b7" />




# Brief
A simple BLE peripheral application that generates mock sensor data and pipes it out via a custom BLE characteristic. It also has a receive characteristic in case you want to configure the device.

This way you can csv it in whatever means you like, preprocess it to ensure signal data is centered, and upload it as training/feature extraction data, even if you don't have a spare serial output for the device.

You can also try to repurpose Nordic UART Service (NUS) to avoid creation of a custom characteristic.


> [!NOTE]
> You could use Enhanced Shockburst (ESB) if you have access to two Nordic devices and pipe out the data potentially faster, with less main.c code content.
> BLE was chosen for this sample for interoperability and to be able to potentially piggyback on an existing BLE application.
> You can see a sample of that [here](https://github.com/droidecahedron/neuton_esb_pull)

# Requirements
## Software
nRF Connect SDK v3.1.0

## Hardware
nRF54L15-DK

# Running the sample
Clone this sample and run `west build -b nrf54l15dk/nrf54l15/cpuapp -p`, then flash your DK with `west flash --recover`.

You can observe the logs to see the 100 samples generated (with the first sample being the iteration), and then piped to the BLE thread.

Connect with any device and enable notifications for the `0x5E85012D` characteristic to see the data come in. You can also write to the `0xDE70CF61` but the sample does nothing but print the data.

# Example output
## Starting up
```
*** Booting nRF Connect SDK v3.1.0-6c6e5b32496e ***
*** Using Zephyr OS v4.1.99-1612683d4010 ***
[00:00:00.000,402] <inf> bt_sdc_hci_driver: SoftDevice Controller build revision: 
                                            fc de 41 eb a2 d1 42 24  00 b5 f8 57 9f ac 9d 9e |..A...B$ ...W....
                                            aa c9 b4 34                                      |...4             
[00:00:00.001,236] <wrn> bt_hci_core: Num of Controller's ACL packets != ACL bt_conn_tx contexts (3 != 10)
[00:00:00.001,590] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
[00:00:00.001,603] <inf> bt_hci_core: HW Variant: nRF54Lx (0x0005)
[00:00:00.001,617] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 252.16862 Build 1121034987
[00:00:00.002,024] <inf> bt_hci_core: HCI transport: SDC
[00:00:00.002,073] <inf> bt_hci_core: Identity: F2:2F:2D:28:32:A4 (random)
[00:00:00.002,088] <inf> bt_hci_core: HCI: version 6.1 (0x0f) revision 0x3069, manufacturer 0x0059
[00:00:00.002,101] <inf> bt_hci_core: LMP: version 6.1 (0x0f) subver 0x3069
[00:00:00.002,107] <inf> neuton_pull_ble: Bluetooth initialized
[00:00:00.002,557] <inf> neuton_pull_ble: Advertising successfully started
 46--- 72 messages dropped ---
 47 48 49 4A 4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A 5B 5C 5D 5E 5F 60 61 62 63
[00:00:01.001,553] <inf> neuton_pull_ble: BLE Thread does not detect an active BLE connection
 46--- 72 messages dropped ---
 ```

 ## LE Phone App (nRF Connect for Mobile) scanning for device
 <img width="25%" alt="image" src="https://github.com/user-attachments/assets/ee963c0f-688c-48eb-ab04-769c7b88421a" />

 ## LE Phone App (nRF Connect for Mobile) receiving data
 <img width="25%" alt="image" src="https://github.com/user-attachments/assets/784c5859-496f-4e4e-88d5-e18f6cbc4c28" />
 
> You will see the first byte go up per "iter" every notification.

# Extras
For a reference application (incl. serial data pull) see the [following](https://github.com/Neuton-tinyML/neuton-nordic-thingy53-ble-remotecontrol/blob/13137e79ba527f2bc68213ea8cd8aedb33ceb253/src/main.c#L135-L137).


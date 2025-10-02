# neuton_ble_pull

# Brief
A simple BLE peripheral application that generates mock sensor data and pipes it out via a custom BLE characteristic. It also has a receive characteristic in case you want to configure the device.

This way you can csv it in whatever means you like, preprocess it to ensure signal data is centered, and upload it as training/feature extraction data, even if you don't have a spare serial output for the device.

You can also try to repurpose Nordic UART Service (NUS) to avoid creation of a custom characteristic.


> [!NOTE]
> You could use Enhanced Shockburst (ESB) if you have access to two Nordic devices and pipe out the data potentially faster, with less main.c code content.
> BLE was chosen for this sample for interoperability and to be able to potentially piggyback on an existing BLE application.
> You can see a sample of ESB throughput [here](https://github.com/droidecahedron/esbperf/tree/main), and use the same logic for the `SENS` portion of main.c of this code sample to use BLE instead.

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


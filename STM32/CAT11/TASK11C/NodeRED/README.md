# Node-RED HTTP Client – CAT11C

## Purpose
This flow uses Node-RED as a second HTTP client for the STM32–Wemos ESP8266 HTTP server.

## Flow
Inject → HTTP Request (GET) → Debug

## Configure
1. Open `CAT11C_NodeRED_Flow.json` in a text editor or import it into Node-RED.
2. In the HTTP Request node, replace `http://<ESP8266_IP>/` with `http://YOUR_WEMOS_IP/`.
3. Deploy the flow.

## Run
1. Make sure the STM32 firmware is running and the Wemos has joined the Wi-Fi network.
2. Ensure the Node-RED host and Wemos are reachable on the network.
3. Click the Inject node.
4. Open the Debug sidebar and inspect the response.

## Expected response
The supplied STM32 firmware returns a static HTML page with an HTTP `200 OK` status. It does not provide live sensor readings.

## Evidence
Place screenshots captured during your own demonstration in `evidence/`. No screenshot was present in the supplied archives, so none has been fabricated or added.

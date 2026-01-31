// Javascript code to set up a websocket.

var m_url_JS = "ws://nanostat.local:81/";
//var m_url_JS = "ws://192.168.1.44:81/";
// var url = "ws://192.168.4.1:1337/";

var m_websocket;
var m_canvas_JS;
var context;
var dataPlot;
var maxDataPoints = 20; // max points in browser cache
var new_binary_data_is_incoming = false; // if true, reset counters, will recieve 3 binary messages with arrays for current voltage time
var amps_array_has_been_received = false;
var volts_array_has_been_received = false;
var time_array_has_been_received = false;
var amps_array = []; // these have to be global and filled one by one, assume browswer has infinite processing power and memory
var volts_array = []; // these have to be global and filled one by one, assume browswer has infinite processing power and memory
var time_array = []; // these have to be global and filled one by one, assume browswer has infinite processing power and memory

// This is called when the page finishes loading
function init() {

    // Assign page elements to variables
    m_canvas_JS = document.getElementById("m_canvas");

    // create chart:

    // dataPlot = new Chart(document.getElementById("m_canvas"), {
    //     type: 'line',
    //     data: {
    //         labels: [],
    //         datasets: [{
    //             data: [],
    //             label: "Temperature (C)",
    //             borderColor: "#3e95cd",
    //             fill: false
    //         }]
    //     },
    //     options: {
    //         scales: {
    //             y: {
    //                 beginAtZero: true
    //             }
    //         }
    //     }
    // });

    // Connect to WebSocket server
    wsConnect(m_url_JS);
}

// Call this to connect to the WebSocket server
function wsConnect(m_url_JS) {

    // Connect to WebSocket server
    m_url_JS = "ws://" + window.location.hostname + ":81/"
    // console.log(m_url_JS);
    m_websocket = new WebSocket(m_url_JS);
    m_websocket.binaryType = "arraybuffer";

    // Assign callbacks
    m_websocket.onopen = function (evt) { onOpen(evt) };
    m_websocket.onclose = function (evt) { onClose(evt) };
    m_websocket.onmessage = function (evt) { onMessage(evt) };
    m_websocket.onerror = function (evt) { onError(evt) };

}

// Called when a WebSocket connection is established with the server
function onOpen(evt) {

    // Log connection state
    console.log("Connected");

    // Enable button
    // button.disabled = false;

    // Get the current state of the LED
    // doSend("getLEDState");
}

// Called when the WebSocket connection is closed
function onClose(evt) {

    // Log disconnection state
    console.log("Disconnected");

    // Disable button
    // button.disabled = true;

    // Try to reconnect after a few seconds
    setTimeout(function () { wsConnect(m_url_JS) }, 2000);
}

// remove excess data from plot
function removeData() {
    dataPlot.data.labels.shift();
    dataPlot.data.datasets[0].data.shift();
}

// add data to plot (through chart object push method...)
function addData(label, data) {
    if (dataPlot.data.labels.length > maxDataPoints) removeData();
    dataPlot.data.labels.push(label);
    dataPlot.data.datasets[0].data.push(data);
    dataPlot.update();
}

// Called when a message is received from the server
function onMessage(evt) {
    console.log("onMessage called");

    if (typeof (evt.data) == "string") {
        console.log("STRING! parsing....");
        console.log("Received: " + evt.data);

        try {
            var m_json_obj = JSON.parse(evt.data);

            var btnUp = document.getElementById("btnUp");
            var btnDown = document.getElementById("btnDown");
            var btnEmg = document.getElementById("btnEmergency");

            if ('floorValue' in m_json_obj) {
                var floorNumDisplay = document.querySelector("#FloorValue .floor-num");
                if (floorNumDisplay) {
                    floorNumDisplay.innerText = m_json_obj.floorValue;
                }
            }

            if (m_json_obj.Moving) {
                if (m_json_obj.Up === true) {
                    btnUp?.classList.add("active");
                } else {
                    btnUp?.classList.remove("active");
                }

                if (m_json_obj.Down === true) {
                    btnDown?.classList.add("active");
                } else {
                    btnDown?.classList.remove("active");
                }
            } else {
                btnUp?.classList.remove("active");
                btnDown?.classList.remove("active");
            }

            // จัดการโหมด Emergency
            if ('Mode' in m_json_obj) {
                if (m_json_obj.Mode === "EMERGENCY") {
                    btnEmg?.classList.add("blink");
                } else {
                    btnEmg?.classList.remove("blink");
                }
            }

            if ('BtwFloor' in m_json_obj) {
                var floorMsg = document.querySelector("#FloorValue .floor-msg");
                
                if (floorMsg) {
                    if (m_json_obj.BtwFloor === true) {
                        floorMsg.classList.add("show"); 
                    } else {
                        floorMsg.classList.remove("show"); 
                    }
                }
            }

        } catch (e) {
            console.error("Error parsing JSON: ", e);
        }
    }
}

function addData(label, data) {
    if (dataPlot.data.labels.length > maxDataPoints) removeData();
    dataPlot.data.labels.push(label);
    dataPlot.data.datasets[0].data.push(data);
    dataPlot.update();
}

// Called when a WebSocket error occurs
function onError(evt) {
    console.log("ERROR: " + evt.data);
}

// Sends a message to the server (and prints it to the console)
function doSend(message) {
    console.log("Sending: " + message);
    websocket.send(message);
}

// Slider calls this to set data rate:
function sendDataRate() {
    var dataRate = document.getElementById("dataRateSlider").value;
    m_websocket.send(dataRate);
    dataRate = 1.0 * dataRate;
    document.getElementById("dataRateLabel").innerHTML = "Rate: " + dataRate.toFixed(2) + "Hz";
}


// Call the init function as soon as the page loads
window.addEventListener("load", init, false);



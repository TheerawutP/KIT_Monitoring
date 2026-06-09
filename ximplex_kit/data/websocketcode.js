// Javascript code to set up a websocket.
var m_url_JS = "ws://" + window.location.hostname + ":81/";
// var m_url_JS = "ws://nanostat.local:81/";
//var m_url_JS = "ws://192.168.1.44:81/";
// var url = "ws://192.168.4.1:1337/";

var m_websocket;
var domElements = {};


// This is called when the page finishes loading
function init() {
domElements.btnUp = document.getElementById("btnUp");
    domElements.btnDown = document.getElementById("btnDown");
    domElements.btnEmg = document.getElementById("btnEmg");
    domElements.emgText = document.getElementById("emgText"); 
    domElements.floorNumDisplay = document.querySelector("#FloorValue .floor-num");
    domElements.floorMsg = document.querySelector("#FloorValue .floor-msg");
    domElements.toastContainer = document.getElementById("toast-container");

    wsConnect();
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
// function onMessage(evt) {
//     const STATE_IDLE = 0;
//     const STATE_RUNNING = 1;
//     const STATE_PENDING = 2;
//     const STATE_PAUSED = 3;
//     const STATE_EMERGENCY = 4;

//     console.log("onMessage called");

//     if (typeof (evt.data) == "string") {
//         console.log("STRING! parsing....");
//         console.log("Received: " + evt.data);

//         try {
//             var m_json_obj = JSON.parse(evt.data);

//             var btnUp = document.getElementById("btnUp");
//             var btnDown = document.getElementById("btnDown");
//             // var btnEmg = document.getElementById("btnEmergency");
            

//             if ('alert' in m_json_obj) {
//                 showToast(m_json_obj.alert, m_json_obj.msg);
//                 return; 
//             }


//             if ('floorValue' in m_json_obj) {
//                 var floorNumDisplay = document.querySelector("#FloorValue .floor-num");
//                 if (floorNumDisplay) {
//                     floorNumDisplay.innerText = m_json_obj.floorValue;
//                 }
//             }

//             if (m_json_obj.state == STATE_RUNNING || m_json_obj.state == STATE_PENDING) {
//                 if (m_json_obj.up === true) {
//                     btnUp?.classList.add("up-active");
//                 } else {
//                     btnUp?.classList.remove("up-active");
//                 }

//                 if (m_json_obj.down === true) {
//                     btnDown?.classList.add("down-active");
//                 } else {
//                     btnDown?.classList.remove("down-active");
//                 }

//             } else {
//                 // ถ้าลิฟต์จอด (IDLE) ให้ดับไฟปุ่มทั้งหมด
//                 btnUp?.classList.remove("up-active");
//                 btnDown?.classList.remove("down-active");
//             }

//             if ('emo' in m_json_obj) {
//                 if (m_json_obj.emo === true) {
//                     // ถ้า true ให้ใส่คลาสไฟกระพริบสีแดง
//                     btnEmg?.classList.add("emerg-active");
//                     emgText?.classList.add("show");
//                 } else {
//                     // ถ้า false ให้ลบคลาสทิ้งไป (ไฟดับ)
//                     btnEmg?.classList.remove("emerg-active");
//                     emgText?.classList.remove("show");
//                 }
//             }

//             // if ('Mode' in m_json_obj) {
//             //     if (m_json_obj.Mode === "EMERGENCY") {
//             //         btnEmg?.classList.add("blink");
//             //     } else {
//             //         btnEmg?.classList.remove("blink");
//             //     }
//             // }

//             if ('btwFloor' in m_json_obj) {
//                 var floorMsg = document.querySelector("#FloorValue .floor-msg");

//                 if (floorMsg) {
//                     if (m_json_obj.btwFloor === true) {
//                         floorMsg.classList.add("show");
//                     } else {
//                         floorMsg.classList.remove("show");
//                     }
//                 }
//             }

//         } catch (e) {
//             console.error("Error parsing JSON: ", e);
//         }
//     }
// }

function onMessage(evt) {
    const STATE_IDLE = 0;
    const STATE_RUNNING = 1;
    const STATE_PENDING = 2;
    const STATE_PAUSED = 3;
    const STATE_EMERGENCY = 4;

    if (typeof (evt.data) === "string") {
        try {
            var obj = JSON.parse(evt.data);

            if ('alert' in obj) {
                showToast(obj.alert, obj.msg);
                return; 
            }

            // 3. เรียกใช้ผ่าน domElements เพื่อความเร็ว
            if ('floorValue' in obj && domElements.floorNumDisplay) {
                if (domElements.floorNumDisplay.innerText !== obj.floorValue.toString()) {
                    domElements.floorNumDisplay.innerText = obj.floorValue;
                }
            }

            if (obj.state === STATE_RUNNING || obj.state === STATE_PENDING) {
                domElements.btnUp?.classList.toggle("up-active", obj.up === true);
                domElements.btnDown?.classList.toggle("down-active", obj.down === true);
            } else {
                domElements.btnUp?.classList.remove("up-active");
                domElements.btnDown?.classList.remove("down-active");
            }

            if ('emo' in obj) {
                domElements.btnEmg?.classList.toggle("emerg-active", obj.emo === true);
                domElements.emgText?.classList.toggle("show", obj.emo === true);
            }

            if ('btwFloor' in obj && domElements.floorMsg) {
                domElements.floorMsg.classList.toggle("show", obj.btwFloor === true);
            }

        } catch (e) {
            console.error("JSON Parse Error: ", e);
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


function showToast(type, message) {
    var container = document.getElementById("toast-container");
    if (!container) return;

    // สร้างกล่อง div ใหม่
    var toast = document.createElement("div");
    
    // ใส่คลาสตามประเภทที่ส่งมา (danger, warning, info) แปลงตัวพิมพ์เล็ก
    toast.className = "toast-msg " + type.toLowerCase();
    
    // ใส่ข้อความ
    toast.innerHTML = "<strong>" + type + "</strong>" + message;

    // เอาไปแปะในหน้าจอ
    container.appendChild(toast);

    // ตั้งเวลาให้ลบตัวเองทิ้งหลังจากผ่านไป 5 วินาที
    setTimeout(function() {
        if(container.contains(toast)){
            container.removeChild(toast);
        }
    }, 5000); 
}
{/* <button onclick= "window.location.href = '/wifi_manager.html'"> Wifi Setup </button> */ }

let temporary_boolean = true;

async function on_Button_UP_pressed() {
  console.log(window.location.hostname)
  try {
    // const response = await fetch("http://nanostat.local/button1pressed", {
    const response = await fetch("/on_Button_UP_pressed", {
      method: "PUT",
      body: JSON.stringify({ on: temporary_boolean }),
      headers: {
        "Content-Type": "application/json",
      },
    });
  } catch (error) {
    alert("Request failed - check the console");
    console.error(error);
  }
}

async function on_Button_DOWN_pressed() {
  try {
    const response = await fetch("/on_Button_DOWN_pressed", {
      method: "PUT",
      body: JSON.stringify({ on: temporary_boolean }),
      headers: {
        "Content-Type": "application/json",
      },
    });
  } catch (error) {
    alert("Request failed - check the console");
    console.error(error);
  }
}

async function on_Button_STOP_pressed() {
  try {
    const response = await fetch("/on_Button_STOP_pressed", {
      method: "PUT",
      body: JSON.stringify({ on: temporary_boolean }),
      headers: {
        "Content-Type": "application/json",
      },
    });
  } catch (error) {
    alert("Request failed - check the console");
    console.error(error);
  }

}
async function on_Button_EMERGENCY_pressed() {
  try {
    const response = await fetch("/on_Button_EMERGENCY_pressed", {
      method: "PUT",
      body: JSON.stringify({ on: temporary_boolean }),
      headers: {
        "Content-Type": "application/json",
      },
    });
  } catch (error) {
    alert("Request failed - check the console");
    console.error(error);
  }
}

function reset_mcu() {
    if (!confirm("ARE YOU SURE YOU WANT TO RESET THE MCU?")) return;

    const data = JSON.stringify({
        reset_mcu: true
    });

    if (m_websocket && m_websocket.readyState === WebSocket.OPEN) {
        m_websocket.send(data);
        console.log("Sent: " + data);
        alert("Sending reset command...");
    } else {
        alert("Error: Cannot connect to lift (WS Disconnected)");
    }
}


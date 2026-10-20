let wsUri = "ws://localhost:19792";
var websocket;
var bDebug = false;
const crlf = '\r\n';
var can; // the canvas object
var ctx; // the canvas context for drawing the meter bar
var s_val; // the s meter value

function loadSettings() {
    // console.log('loadSettings....');
    document.getElementById("trace").style.visibility = "hidden"; 
    document.getElementById("cleartext").style.visibility = "hidden";
    document.getElementById("help").style.visibility = "hidden";
    document.getElementById("debug").checked = false;
    can = document.getElementById("smeter");    
    ctx = can.getContext("2d");
    s_val = document.getElementById("sval");
    // console.log(ctx);
}

function initializeWebSocketListener(ws) {
    ws.addEventListener("open", () => {
		console.log("CONNECTED");        
        sendCommand("version");
        sendCommand("tovfoa");
        sendCommand("vfoafreq 7163000");
        sendCommand("mode lsb");
        sendCommand("user voll");
        sendCommand("?mode");
        //sendCommand("debug on");
        
        // use the pingInterval as a watchdog
        // if required and handle any errors that
        // arise if the socket has been dropped
		// pingInterval = setInterval(() => {
		// 	ws.send("ping " + counter);
        //     counter++;
        //     if (counter == 20) {
        //         ws.close();
        //     }
		// }, 1000);
	});

	ws.addEventListener("close", () => {
		console.log("DISCONNECTED");
		// clearInterval(pingInterval);
	});

	ws.addEventListener("message", async (e) => {
        let out = await e.data.text();
        out = out.toString();
		if(out != null) {
            // console.log(out);
            if(out.startsWith('RM0')) {
                //console.log(out);
                const val = out.slice(3, 6);
                showSMeter(val);
            }
            else if(out.startsWith('mode ')) {
                var str = out.trim();
                var parts = str.split(' ');
                var len = parts.length;                
                str = parts[len - 1] + '';
                document.getElementById('modelist').value = str;
            }
            else if(out.startsWith('ARCON version: ')) {
                var str = out.trim();
                var parts = str.split(' -- ');
                var name = parts[1] + '';
                document.getElementById('tabletitle').innerHTML = "ARCON Rules the Radio -- " + name;
            }
        }
        var currtext = document.getElementById("trace").innerHTML;
        var doc = document.getElementById("trace");
        doc.innerHTML = currtext + crlf + out;
        doc.scrollTop = doc.scrollHeight - doc.clientHeight;
	});

	ws.addEventListener("error", (e) => {
		console.log(`ERROR`);
	});
}

window.addEventListener("pageshow", (event) => {
	if (event.persisted) {
        console.log('starting websocket...');
        websocket = new WebSocket(wsUri);
		initializeWebSocketListener(websocket);
	}
});

function sendCommand(data) { websocket.send(data); }

function setDebug(on) { 
    bDebug = on;
    sendCommand('debug ' + bDebug?'on':'off');
//    console.log("on: " + bDebug);
    if(on) {
        document.getElementById("trace").style.visibility = "visible"; 
        document.getElementById("cleartext").style.visibility = "visible"; 
        document.getElementById("help").style.visibility = "visible";
    }
    else {
        document.getElementById("trace").style.visibility = "hidden"; 
        document.getElementById("cleartext").style.visibility = "hidden"; 
        document.getElementById("help").style.visibility = "hidden";
    }
}

function setMode(modename) {
    if(modename === '')return;
    sendCommand('mode ' + modename);
}

function showSMeter(val) {
    //console.log('meter: ' + val);
    if(ctx == null) {
        ctx = can.getContext("2d");
    }
    ival = parseInt(val);
    // console.log("meter:" + ival);
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, 255, 16);
    ctx.fillStyle = "green";
    ctx.fillRect(0, 0, ival, 16);

    switch(true) {
        case (ival > 173): s_val.innerHTML = "S9+";break;
        case (ival > 133): s_val.innerHTML = "S9";break;
        case (ival > 113): s_val.innerHTML = "S8";break;
        case (ival > 103): s_val.innerHTML = "S7";break;
        case (ival > 87):  s_val.innerHTML = "S6";break;
        case (ival > 75):  s_val.innerHTML = "S5";break;
        case (ival > 57):  s_val.innerHTML = "S4";break;
        case (ival > 42):  s_val.innerHTML = "S3";break;
        case (ival > 23):  s_val.innerHTML = "S1"; break;
        default: s_val.innerHTML = "S0";break;
    }
}

console.log("OPENING");
websocket = new WebSocket(wsUri);
initializeWebSocketListener(websocket);

// Close the websocket when the user leaves.
window.addEventListener("pagehide", () => {
	if (websocket) {
		console.log("CLOSING");
		websocket.close();
		websocket = null;
		// window.clearInterval(pingInterval);
	}
});
// Input functionality
log('Disconnected from server');

document.getElementById('connectBtn').addEventListener('click', function() {
    var inputField = document.createElement('input');
    inputField.setAttribute('type', 'text');
    inputField.setAttribute('id', 'inputField');
    inputField.setAttribute('placeholder', 'Enter Device Endpoint');
    this.parentElement.appendChild(inputField);
    inputField.focus();

    inputField.addEventListener('keypress', function(event) {
      if (event.key === 'Enter') {
        var endpoint = inputField.value;
        const connectBtn = document.getElementById('connectBtn');
        inputField.remove();
        try{
            const socket = new WebSocket(endpoint);
            log('Connecting...');

            // Handle WebSocket events
            socket.onopen = () => {
                log('Connected to server');
                connectBtn.disabled = true;
                socket.send("log");
                updateStatus(true);
            };

            socket.onclose = () => {
                log('Disconnected from server');
                connectBtn.disabled = false;
                updateStatus(false);
            };

            socket.onmessage = (event) => {
                const message = event.data;
                log1(message);
            };

            socket.onerror = function(event) {
                connectBtn.disabled = false;
                log('Error occured');
            };
            } catch(error){
                log(error);
            }
      }
    });
  });


// Function to append log entries to the command line interface
function log(message) {
    const commandLine = document.getElementById('command-line');
    const logEntry = document.createElement('div');
    logEntry.classList.add('log-entry');
    logEntry.textContent = message;
    commandLine.appendChild(logEntry);
    commandLine.scrollTop = commandLine.scrollHeight; // Scroll to bottom
}

// Function to update connection status in the sidebar
function updateStatus(connected) {
    const statusElement = document.getElementById('status');
    statusElement.textContent = connected ? 'Status: Connected' : 'Status: Disconnected';
}

function updateStatus(connected) {
    const statusElement = document.getElementById('status');
    if (connected) {
        statusElement.textContent = 'Connected';
        statusElement.style.color = '#0f0';
    } else {
        statusElement.textContent = 'Disconnected';
        statusElement.style.color = 'red';
    }
}


function log1(message) {
    const commandLine = document.getElementById('command-line');
    
    // Extract log level and message from the input message
    const match = message.match(/\[(\d+)\;(\d+)m(.+?)\u001b/g);
    if (!match) {
        return; // Invalid log format, do not log
    }
    const logLevel = match[0][2]; // Get the log level (e.g., 'W', 'I', 'E')
    const logMessage = match[0].substring(match[0].indexOf('m') + 1, match[0].lastIndexOf('\u001b')); // Get the log message
    
    // Create log entry element
    const logEntry = document.createElement('div');
    logEntry.classList.add('log-entry');
    
    // Apply default style
    logEntry.style.color = 'white';

    // Apply additional styles based on the message content
    if (message.includes('\u001b[0;33m')) { // Yellow color
        logEntry.style.color = 'yellow';
    } else if (message.includes('\u001b[0;32m')) { // Green color
        logEntry.style.color = '#0f0';
    } else if (message.includes('\u001b[0;31m')) { // Red color
        logEntry.style.color = 'red';
    }
    
    // Set log message
    logEntry.textContent = logMessage;
    
    // Append log entry to command line
    commandLine.appendChild(logEntry);
    commandLine.scrollTop = commandLine.scrollHeight; // Scroll to bottom
}


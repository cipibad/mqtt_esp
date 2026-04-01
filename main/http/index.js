const MAX_RELAYS = 4;
const discoveredRelays = [];
let actuatorAvailable = false;
let otaAvailable = false;

function hideElement(id) {
    const el = document.getElementById(id);
    if (el) el.style.display = 'none';
}

function showElement(id) {
    const el = document.getElementById(id);
    if (el) el.style.display = '';
}

hideElement('relays-section');
hideElement('actuator-section');
hideElement('ota-section');

function checkActuatorSupport() {
    fetch('status/actuator')
        .then(response => {
            if (response.ok) {
                actuatorAvailable = true;
                showElement('actuator-section');
                return response.json();
            }
            throw new Error('Actuator not available');
        })
        .then(data => {
            updateActuatorActivity(data);
        })
        .catch(error => {
            actuatorAvailable = false;
            hideElement('actuator-section');
        });
}

function updateActuatorActivity(data) {
    if (!actuatorAvailable) return;
    if (data.value == 'Closing') {
        document.getElementById("activitate").textContent = 'Inchidere in progres';
    } else if (data.value == 'Opening') {
        document.getElementById("activitate").textContent = 'Deschidere in progres';
    } else if (data.value == 'Off') {
        document.getElementById("activitate").textContent = 'Fara activitate';
    } else {
        document.getElementById("activitate").textContent = 'Eroare !!';
    }
}

function discoverRelays() {
    let checkCount = 0;
    for (let i = 0; i < MAX_RELAYS; i++) {
        fetch(`relay/${i}`)
            .then(response => {
                if (response.ok) {
                    return response.text();
                }
                throw new Error('Relay not found');
            })
            .then(status => {
                if (!discoveredRelays.includes(i)) {
                    discoveredRelays.push(i);
                    createRelayUI(i);
                    showElement('relays-section');
                }
                updateRelayStatus(i, status);
            })
            .catch(error => {
                // Relay doesn't exist, ignore
            })
            .finally(() => {
                checkCount++;
                if (checkCount === MAX_RELAYS && discoveredRelays.length === 0) {
                    hideElement('relays-section');
                }
            });
    }
}

function createRelayUI(relayId) {
    const container = document.getElementById('relays-container');
    const relayDiv = document.createElement('div');
    relayDiv.className = 'relay-control';
    relayDiv.id = `relay-${relayId}`;
    relayDiv.innerHTML = `
        <div class="relay-header">Releu ${relayId}</div>
        <div class="relay-status">Status: <span id="relay-status-${relayId}">--</span></div>
        <div class="relay-buttons">
            <button class="relay-button relay-on" onclick="setRelay(${relayId}, 'ON')">ON</button>
            <button class="relay-button relay-off" onclick="setRelay(${relayId}, 'OFF')">OFF</button>
        </div>
    `;
    container.appendChild(relayDiv);
}

function updateRelayStatus(relayId, status) {
    const statusSpan = document.getElementById(`relay-status-${relayId}`);
    if (statusSpan) {
        statusSpan.textContent = status;
        statusSpan.className = status === 'ON' ? 'status-on' : 'status-off';
    }
}

function fetchRelayStatus(relayId) {
    fetch(`relay/${relayId}`)
        .then(response => {
            if (response.ok) {
                return response.text();
            }
            throw new Error('Network error');
        })
        .then(status => {
            updateRelayStatus(relayId, status);
        })
        .catch(error => {
            console.error(`Error fetching relay ${relayId}:`, error);
        });
}

function setRelay(relayId, command) {
    fetch(`relay/${relayId}`, {
        method: 'POST',
        headers: {
            'Content-Type': 'text/plain'
        },
        body: command
    })
    .then(response => {
        if (response.ok) {
            return response.text();
        }
        throw new Error('Network error');
    })
    .then(result => {
        console.log(`Relay ${relayId} set to ${command}: ${result}`);
        fetchRelayStatus(relayId);
    })
    .catch(error => {
        console.error(`Error setting relay ${relayId}:`, error);
    });
}

function fetchActivitate() {
    if (!actuatorAvailable) return;
    fetch('status/actuator')
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        updateActuatorActivity(data);
    })
    .catch(error => {
        document.getElementById("activitate").textContent = 'Nu se pot actualiza datele, verificati reteaua wifi';
        console.error('There was a problem with the fetch operation:', error);
    });
}

fetchActivitate();

setInterval(fetchActivitate, 2000);

function fetchPozitie() {
    if (!actuatorAvailable) return;
    fetch('level/actuator')
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        if (!actuatorAvailable) return;
        if (data.value == 0) {
            document.getElementById("pozitie").textContent = 'Inchisa';
        } else if (data.value == 1) {
            document.getElementById("pozitie").textContent = 'Deschisa un sfert';
        } else if (data.value == 2) {
            document.getElementById("pozitie").textContent = 'Deschisa jumatate';
        } else if (data.value == 3) {
            document.getElementById("pozitie").textContent = 'Deschisa trei sferturi';
        } else if (data.value == 4) {
            document.getElementById("pozitie").textContent = 'Deschisa complet';
        } else {
            document.getElementById("pozitie").textContent = 'Eroare';
        }
    })
    .catch(error => {
        document.getElementById("pozitie").textContent = 'Nu se pot actualiza datele, verificati reteaua wifi';
        console.error('There was a problem with the fetch operation:', error);
    });
}

fetchPozitie();

setInterval(fetchPozitie, 2000);

function sendPostRequest(command, period) {
    const url = 'action/actuator'; // Înlocuiți cu URL-ul dvs. de destinație

    const data = {
        command,
        period
    };

    fetch(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(data)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        console.log('POST request succeeded with JSON response:', data);
        // Aici puteți face ce doriți cu răspunsul primit
    })
    .catch(error => {
        console.error('There was a problem with the POST request:', error);
    });
}


function openActuator(type) {
    if (!actuatorAvailable) return;
    var period = type;
    var command = 'open';
    sendPostRequest(command, period);
}

function closeActuator(type) {
    if (!actuatorAvailable) return;
    var period = type;
    var command = 'close';
    sendPostRequest(command, period);
}

checkActuatorSupport();
discoverRelays();
checkOtaSupport();

setInterval(() => {
    discoveredRelays.forEach(fetchRelayStatus);
}, 2000);

setInterval(() => {
    if (otaAvailable) checkOtaSupport();
}, 5000);
setInterval(checkOtaSupport, 5000);

function checkOtaSupport() {
    fetch('status/ota')
        .then(response => {
            if (response.ok) {
                otaAvailable = true;
                showElement('ota-section');
                return response.text();
            }
            throw new Error('OTA not available');
        })
        .then(status => {
            updateOtaStatus(status);
            fetchVersion();
        })
        .catch(() => {
            otaAvailable = false;
            hideElement('ota-section');
        });
}

function fetchVersion() {
    fetch('status/version')
        .then(response => {
            if (response.ok) return response.json();
            throw new Error('Version not available');
        })
        .then(data => {
            document.getElementById('ota-version').textContent = data.version;
        })
        .catch(error => {
            console.error('Error fetching version:', error);
        });
}

function updateOtaStatus(status) {
    const statusSpan = document.getElementById('ota-status');
    if (!statusSpan) return;
    
    let displayStatus;
    switch(status) {
        case 'ongoing': displayStatus = 'In desfasurare...'; break;
        case 'success': displayStatus = 'Succes'; break;
        case 'failed': displayStatus = 'Eroare'; break;
        default: displayStatus = 'Pregatit'; break;
    }
    statusSpan.textContent = displayStatus;
    statusSpan.className = 'ota-status-' + status;
}

function triggerOta() {
    if (!otaAvailable) return;
    if (!confirm('Sigur doriti sa actualizati firmware-ul? Dispozitivul va reporni.')) return;
    
    fetch('action/ota', { method: 'POST' })
        .then(response => response.text())
        .then(result => {
            alert('Actualizare pornita: ' + result);
            updateOtaStatus('ongoing');
        })
        .catch(error => {
            alert('Eroare la pornirea actualizarii');
            console.error('OTA trigger error:', error);
        });
}


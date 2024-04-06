function fetchActivitate() {
    // Make a fetch request to the REST API endpoint
    fetch('status/actuator')
    .then(response => {
        // Check if the response is successful
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        // Parse the JSON response
        return response.json();
    })
    .then(data => {
        // Update the content of the span element with the data received
        if (data.value == 'Closing') {
            document.getElementById("activitate").textContent = 'Inchidere in progres';
        } else if (data.value == 'Opening') {
            document.getElementById("activitate").textContent = 'Deschidere in progres';
        } else if (data.value == 'Off') {
            document.getElementById("activitate").textContent = 'Fara activitate';
        } else {
            document.getElementById("activitate").textContent = 'Eroare !!';
        }
    })
    .catch(error => {
        // Handle any errors that occur during the fetch
        document.getElementById("activitate").textContent = 'Nu se pot actualiza datele, verificati reteaua wifi';
        console.error('There was a problem with the fetch operation:', error);
    });
}

fetchActivitate();

// Set interval to call fetchData every 5 seconds (for example)
setInterval(fetchActivitate, 2000); // 5000 milliseconds = 5 seconds

function fetchPozitie() {
    // Make a fetch request to the REST API endpoint
    fetch('level/actuator')
    .then(response => {
        // Check if the response is successful
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        // Parse the JSON response
        return response.json();
    })
    .then(data => {
        // Update the content of the span element with the data received
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
        // Handle any errors that occur during the fetch
        document.getElementById("pozitie").textContent = 'Nu se pot actualiza datele, verificati reteaua wifi';
        console.error('There was a problem with the fetch operation:', error);
    });
}

fetchPozitie();

// Set interval to call fetchData every 5 seconds (for example)
setInterval(fetchPozitie, 2000); // 5000 milliseconds = 5 seconds

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


// JavaScript
function openActuator(type) {
    // Implementează funcționalitatea pentru deschiderea porții
    var period = type;
    var command = 'open';
    sendPostRequest(command, period);
}

  function closeActuator(type) {
    // Implementează funcționalitatea pentru închiderea porții
    var period = type;
    var command = 'close';
    sendPostRequest(command, period);
  }


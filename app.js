/**
 * =====================================================
 * SMART MEDICATION BOX - PRODUCTION LOGIC
 * =====================================================
 */

// ⚠️ UPDATED CONFIG
const firebaseConfig = {
    apiKey: "AIzaSyBfoXX_E1568WwAR6sCls_5o9L5h1FgZqc",
    authDomain: "smart-medication-box-e8e5b.firebaseapp.com",
    databaseURL: "https://smart-medication-box-e8e5b-default-rtdb.firebaseio.com",
    projectId: "smart-medication-box-e8e5b",
    storageBucket: "smart-medication-box-e8e5b.firebasestorage.app",
    messagingSenderId: "1011772416994",
    appId: "1:1011772416994:web:e716aec022d5c8d592cfbb"
};

try {
    firebase.initializeApp(firebaseConfig);
} catch (e) {
    console.error("Firebase Init Error", e);
}

const database = firebase.database();
const rootRef = database.ref('medication_box');

// Initialize App Directly (No Auth Guard)
document.addEventListener('DOMContentLoaded', () => {
    initApp();
});

function initApp() {
    [1, 2, 3, 4].forEach(id => {
        initCompartmentForm(id);
        setupListener(id);
    });
}

function initCompartmentForm(id) {
    const form = document.querySelector(`form[data-compartment="${id}"]`);
    const list = form.querySelector('.medicines-list');
    const addBtn = form.querySelector('.btn-add-med');

    // Add Med
    addBtn.addEventListener('click', () => {
        addMedicineRow(list);
    });

    // Save
    form.addEventListener('submit', (e) => {
        e.preventDefault();
        saveCompartment(id, form);
    });
}

function addMedicineRow(listContainer, name = "", count = 1) {
    const row = document.createElement('div');
    row.className = 'medicine-row';

    const nameInput = document.createElement('input');
    nameInput.type = 'text';
    nameInput.className = 'med-name';
    nameInput.placeholder = 'Med Name';
    nameInput.value = name;
    nameInput.required = true;

    const countInput = document.createElement('input');
    countInput.type = 'number';
    countInput.className = 'med-count';
    countInput.placeholder = '#';
    countInput.min = '1';
    countInput.value = count;
    countInput.required = true;

    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.className = 'btn-remove-med';
    removeBtn.innerHTML = '✕'; // Using HTML entity for X
    removeBtn.onclick = function () {
        if (confirm('Delete this medicine?')) {
            row.remove();
        }
    };

    row.appendChild(nameInput);
    row.appendChild(countInput);
    row.appendChild(removeBtn);

    listContainer.appendChild(row);
}

function saveCompartment(id, form) {
    const h = form.querySelector('.hour').value.padStart(2, '0');
    const m = form.querySelector('.minute').value.padStart(2, '0');
    const ampm = form.querySelector('.time-ampm').value;
    const buzzer = form.querySelector('.buzzer-toggle').checked;

    // Parse Medicines
    const meds = [];
    form.querySelectorAll('.medicine-row').forEach(row => {
        const name = row.querySelector('.med-name').value;
        const count = row.querySelector('.med-count').value;
        if (name) {
            meds.push({
                name: name,
                tablets: parseInt(count) || 1
            });
        }
    });

    const data = {
        time: `${h}:${m} ${ampm}`,
        buzzer: buzzer,
        medicine_taken: false, // Reset taken status on update
        medicines: meds,
        last_updated: firebase.database.ServerValue.TIMESTAMP
    };

    rootRef.child(`compartment_${id}`).set(data)
        .then(() => alert(`Compartment ${id} Schedule Saved!`))
        .catch(e => alert("Error: " + e.message));
}

function setupListener(id) {
    // Realtime Listener
    rootRef.child(`compartment_${id}`).on('value', (snapshot) => {
        const data = snapshot.val();
        if (!data) return;

        const statusEl = document.getElementById(`status-${id}`);
        // We do NOT overwrite inputs on realtime updates to avoid disturbing user typing
        // We ONLY update status indicators

        if (data.medicine_taken) {
            statusEl.textContent = "✅ Taken Today";
            statusEl.className = "status-indicator status-taken";
        } else {
            statusEl.textContent = `⏳ Scheduled: ${data.time}`;
            statusEl.className = "status-indicator status-pending";
        }
    });

    // One-time load for inputs
    rootRef.child(`compartment_${id}`).once('value', (snapshot) => {
        const data = snapshot.val();
        if (!data) return;

        const form = document.querySelector(`form[data-compartment="${id}"]`);

        // Load Time
        if (data.time) {
            const [timeStr, ampm] = data.time.split(' ');
            const [h, m] = timeStr.split(':');
            form.querySelector('.hour').value = h;
            form.querySelector('.minute').value = m;
            form.querySelector('.time-ampm').value = ampm;
        }

        // Load Buzzer
        form.querySelector('.buzzer-toggle').checked = (data.buzzer === true);

        // Load Medicines
        const list = form.querySelector('.medicines-list');
        list.innerHTML = '';
        if (data.medicines) {
            data.medicines.forEach(m => addMedicineRow(list, m.name, m.tablets));
        } else {
            addMedicineRow(list); // Default empty row
        }
    });
}

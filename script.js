const fileInput = document.getElementById('fileInput');
const fileLabel = document.getElementById('fileLabel');

// --- 1. UI UPDATES ---
if (fileInput) {
    fileInput.onchange = () => {
        if (fileInput.files.length > 0) {
            if (fileInput.files.length > 1) {
                fileLabel.innerText = "Folder: " + fileInput.files[0].webkitRelativePath.split('/')[0];
            } else {
                fileLabel.innerText = "File: " + fileInput.files[0].name;
            }
        }
    };
}

function updateMode() {
    const isFolder = document.querySelector('input[name="compType"]:checked').value === 'folder';
    if (isFolder) {
        fileInput.setAttribute('webkitdirectory', '');
        fileInput.setAttribute('directory', '');
        fileLabel.innerText = "Select Folder";
    } else {
        fileInput.removeAttribute('webkitdirectory');
        fileInput.removeAttribute('directory');
        fileLabel.innerText = "Select File";
    }
    fileInput.value = '';
}

// --- 2. ENGINE HANDSHAKE (COMPRESS/DECOMPRESS) ---
async function handleAction(event, route) {
    event.preventDefault();
    
    const submitBtn = document.getElementById('submitBtn');
    const btnText = document.getElementById('btnText');
    const spinner = document.getElementById('spinner');
    const status = document.getElementById('status');

    // 1. Enter Loading State
    submitBtn.classList.add('btn-loading');
    spinner.style.display = 'block';
    btnText.innerText = "Processing...";
    status.innerText = "Engine is running, please wait...";

    const formData = new FormData();
    const password = document.getElementById('password').value;
    formData.append('password', password);

    if (route === '/compress') {
        const format = document.querySelector('input[name="format"]:checked').value;
        const isFolder = document.querySelector('input[name="compType"]:checked').value === 'folder';
        formData.append('format', format);
        if (isFolder) {
            formData.append('folderName', fileInput.files[0].webkitRelativePath.split('/')[0]);
            for (const f of fileInput.files) formData.append('files', f, f.webkitRelativePath);
        } else {
            formData.append('files', fileInput.files[0]);
        }
    } else {
        formData.append('files', fileInput.files[0]);
    }

    try {
        const res = await fetch(route, { method: 'POST', body: formData });
        if (res.status === 401) {
            status.innerText = "Wrong Password!";
            status.style.color = "red";
            return;
        }
        if (!res.ok) throw new Error();

        const disposition = res.headers.get('Content-Disposition');
        let filename = "file";
        if (disposition && disposition.includes('filename=')) {
            filename = disposition.split('filename=')[1].replace(/"/g, '').trim();
        }

        const blob = await res.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename; 
        document.body.appendChild(a);
        a.click();
        a.remove();

        status.innerText = "Success!";
        status.style.color = "green";
    } catch (err) {
        status.innerText = "Error encountered.";
        status.style.color = "red";
    }finally {
        // 3. Reset Button State
        submitBtn.classList.remove('btn-loading');
        spinner.style.display = 'none';
        btnText.innerText = route === '/compress' ? "Compress Now" : "Decompress Now";
    }
}



// --- 3. SQL OTP & HISTORY REVEAL ---
async function requestOtp() {
    const res = await fetch('/send-otp', { method: 'POST' });
    if (res.ok) {
        document.getElementById('otpModal').style.display = 'block';
    } else {
        alert("Failed to initiate security check.");
    }
}

async function verifyAndReveal() {
    const enteredOtp = document.getElementById('otpInput').value;
    const res = await fetch('/verify-otp', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enteredOtp })
    });

    if (res.ok) {
        // Unblur the sensitive SQL data in the table
        document.querySelectorAll('.pass-cell').forEach(el => {
            el.classList.remove('pass-hidden');
            el.style.filter = "none"; 
        });
        closeModal();
    } else {
        alert("Incorrect or expired code. Please try again.");
    }
}

function closeModal() {
    const modal = document.getElementById('otpModal');
    if (modal) modal.style.display = 'none';
    const input = document.getElementById('otpInput');
    if (input) input.value = '';
}

// --- 4. EVENT LISTENERS ---
const compForm = document.getElementById('compForm');
const decompForm = document.getElementById('decompForm');

if (compForm) compForm.onsubmit = (e) => handleAction(e, '/compress');
if (decompForm) decompForm.onsubmit = (e) => handleAction(e, '/decompress');
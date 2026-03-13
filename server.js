const express = require('express');
const multer = require('multer');
const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');
const archiver = require('archiver');
const tar = require('tar');
const sqlite3 = require('sqlite3').verbose();
const bcrypt = require('bcrypt');
const session = require('express-session');
const nodemailer = require('nodemailer');

require('dotenv').config();

// Determine the correct C++ engine command based on the OS
const engineCmd = process.platform === 'win32' ? 'huffman.exe' : './huffman';


const app = express();
const port = 3000;

// 1. DATABASE CONNECTION
const db = new sqlite3.Database('./database.db');



// --- NEW: AUTO-MIGRATE COLUMNS ---
db.serialize(() => {
    // Attempt to add the columns. If they already exist, SQLite will just quietly ignore it.
    db.run(`ALTER TABLE users ADD COLUMN otp_code TEXT`, (err) => {});
    db.run(`ALTER TABLE users ADD COLUMN otp_expiry TEXT`, (err) => {});
});

// 2. MIDDLEWARE
app.use(express.json());
app.use(express.static(__dirname)); 
app.use(session({
   secret: process.env.SESSION_SECRET, 
    resave: false,
    saveUninitialized: false
}));

const upload = multer({ dest: 'uploads/' });
if (!fs.existsSync('uploads')) fs.mkdirSync('uploads');

// 3. EMAIL CONFIGURATION
   const transporter = nodemailer.createTransport({
    service: 'gmail',
    auth: {
        user: process.env.GMAIL_USER,
        pass: process.env.GMAIL_PASS
    }
});


// 4. HELPER FUNCTIONS
function cleanup(files = [], dirs = []) {
    files.forEach(f => { if (fs.existsSync(f)) fs.unlinkSync(f); });
    dirs.forEach(d => { if (fs.existsSync(d)) fs.rmSync(d, { recursive: true, force: true }); });
}

function generateOTP() {
    return Math.floor(100000 + Math.random() * 900000).toString();
}

// --- PROFILE MANAGEMENT ROUTES ---

// 1. Update Password (Logged-in User)
app.post('/update-password', async (req, res) => {
    if (!req.session.userId) return res.status(401).send("Unauthorized");

    const { currentPassword, newPassword } = req.body;

    // First, verify the current password
    db.get(`SELECT password FROM users WHERE id = ?`, [req.session.userId], async (err, user) => {
        if (err || !user) return res.status(500).send("Database Error");

        const match = await bcrypt.compare(currentPassword, user.password);
        if (!match) return res.status(401).send("Current password is incorrect.");

        // Hash the new password and update the database
        try {
            const hashedNew = await bcrypt.hash(newPassword, 10);
            db.run(`UPDATE users SET password = ? WHERE id = ?`, [hashedNew, req.session.userId], (updateErr) => {
                if (updateErr) return res.status(500).send("Failed to update password.");
                res.send("Password updated successfully!");
            });
        } catch (e) {
            res.status(500).send("Encryption error.");
        }
    });
});

// 2. Delete Account & Clean Up History
app.post('/delete-account', (req, res) => {
    if (!req.session.userId) return res.status(401).send("Unauthorized");

    // Start a transaction-like process to maintain relational integrity
    // First, delete their history records
    db.run(`DELETE FROM history WHERE user_id = ?`, [req.session.userId], (err) => {
        if (err) return res.status(500).send("Failed to clear history.");

        // Second, delete the user account
        db.run(`DELETE FROM users WHERE id = ?`, [req.session.userId], (err) => {
            if (err) return res.status(500).send("Failed to delete account.");
            
            // Destroy the session so they are logged out
            req.session.destroy();
            res.clearCookie('connect.sid');
            res.send("Account deleted.");
        });
    });
});

// 5. AUTHENTICATION ROUTES
app.post('/register', async (req, res) => {
    const { username, password, email } = req.body;
    try {
        const hashedPassword = await bcrypt.hash(password, 10);
        db.run(`INSERT INTO users (username, password, email) VALUES (?, ?, ?)`, 
        [username, hashedPassword, email], function(err) {
            if (err) return res.status(400).send("User or Email already exists.");
            res.send("Success");
        });
    } catch (e) { res.status(500).send("Server Error"); }
});

app.post('/login', (req, res) => {
    const { username, password } = req.body;
    db.get(`SELECT * FROM users WHERE username = ?`, [username], async (err, user) => {
        if (err || !user) return res.status(401).send("User not found.");
        const match = await bcrypt.compare(password, user.password);
        if (match) {
            req.session.userId = user.id;
            req.session.username = user.username;
            res.send("Success");
        } else { res.status(401).send("Wrong password."); }
    });
});


app.get('/logout', (req, res) => {
    // 1. Destroy the session on the server
    req.session.destroy((err) => {
        if (err) {
            console.log("Logout Error:", err);
            return res.redirect('/index.html');
        }
        // 2. Clear the session cookie
        res.clearCookie('connect.sid'); 
        // 3. Redirect to login page
        res.redirect('/login.html');
    });
});
app.get('/user-info', (req, res) => {
    if (req.session.username) {
        res.json({ username: req.session.username });
    } else {
        res.status(401).send("Not logged in");
    }
});

// --- PASSWORD RESET: STEP 1 (Request OTP) ---
app.post('/request-password-reset', (req, res) => {
    const { email } = req.body;

    // 1. Find the user by email
    db.get(`SELECT id FROM users WHERE email = ?`, [email], (err, user) => {
        if (err || !user) return res.status(404).send("Email not found in our records.");

        // 2. Generate OTP and Expiry
        const otp = Math.floor(100000 + Math.random() * 900000).toString();
        const expiry = new Date(Date.now() + 15 * 60000).toISOString(); // 15 mins

        // 3. DML Update: Store the OTP for this user
        db.run(`UPDATE users SET otp_code = ?, otp_expiry = ? WHERE id = ?`, 
        [otp, expiry, user.id], (updateErr) => {
            if (updateErr) return res.status(500).send("Database error.");

            // 4. Send Email
            const mailOptions = {
                from: '"Huffman Secure" <your-email@gmail.com>',
                to: email,
                subject: 'Your Password Reset Code',
                text: `Use this code to reset your password: ${otp}. It expires in 15 minutes.`
            };

            transporter.sendMail(mailOptions, (mailErr) => {
                if (mailErr) return res.status(500).send("Failed to send email.");
                res.send("Success");
            });
        });
    });
});

// --- PASSWORD RESET: STEP 2 (Verify & Update) ---
app.post('/reset-password', async (req, res) => {
    const { email, otp, newPassword } = req.body;

    db.get(`SELECT otp_code, otp_expiry FROM users WHERE email = ?`, [email], async (err, row) => {
        if (err || !row) return res.status(404).send("User not found.");

        const now = new Date();
        const expiry = new Date(row.otp_expiry);

        // Verify OTP matches and hasn't expired
        if (row.otp_code === otp && now < expiry) {
            try {
                const hashedPassword = await bcrypt.hash(newPassword, 10);
                
                // Update password and CLEAR the OTP so it can't be used again
                db.run(`UPDATE users SET password = ?, otp_code = NULL, otp_expiry = NULL WHERE email = ?`, 
                [hashedPassword, email], (updateErr) => {
                    if (updateErr) return res.status(500).send("Failed to update password.");
                    res.send("Success");
                });
            } catch (hashErr) {
                res.status(500).send("Encryption error.");
            }
        } else {
            res.status(400).send("Invalid or expired reset code.");
        }
    });
});

app.post('/send-otp', (req, res) => {
    if (!req.session.userId) return res.status(401).send("Unauthorized");
    
    const otp = generateOTP();
    const expiry = new Date(Date.now() + 10 * 60000).toISOString();

    // 1. Update the Database
    db.run(`UPDATE users SET otp_code = ?, otp_expiry = ? WHERE id = ?`, 
    [otp, expiry, req.session.userId], (err) => {
        if (err) return res.status(500).send("Database Error");
        
        // 2. Fetch the user's email to ensure we have the right destination
        db.get(`SELECT email FROM users WHERE id = ?`, [req.session.userId], (err, user) => {
            if (err || !user) return res.status(500).send("User email not found.");

            console.log(`📡 Attempting to send OTP ${otp} to ${user.email}...`);

            // 3. The Mail Handshake
            const mailOptions = {
                from: '"Huffman server" <susenkumariitg@gmail.com>',
                to: user.email,
                subject: 'Your Security Code',
                text: `Your OTP is: ${otp}. It expires in 10 minutes.`
            };

            transporter.sendMail(mailOptions, (error, info) => {
                if (error) {
                    console.error("❌ NODEMAILER ERROR:", error.message);
                    return res.status(500).send("Mail failed: " + error.message);
                }
                console.log("✅ MAIL SENT SUCCESSFULLY:", info.response);
                res.send("OTP Sent to your email!");
            });
        });
    });
});

app.post('/verify-otp', (req, res) => {
    const { enteredOtp } = req.body;
    const userId = req.session.userId;

    if (!userId) return res.status(401).send("Session expired. Please login again.");

    const sql = `SELECT otp_code, otp_expiry FROM users WHERE id = ?`;

    db.get(sql, [userId], (err, row) => {
        if (err || !row) return res.status(500).send("Database Error");

        const now = new Date();
        const expiry = new Date(row.otp_expiry);

        console.log(`[DEBUG] Comparing: ${row.otp_code} vs ${enteredOtp}`);
        console.log(`[DEBUG] Time Check: Now(${now}) < Expiry(${expiry}) is ${now < expiry}`);

        if (row.otp_code === enteredOtp && now < expiry) {
            // SUCCESS: Clear the OTP after successful use
            db.run(`UPDATE users SET otp_code = NULL WHERE id = ?`, [userId]);
            res.send("Verified");
        } else {
            res.status(400).send("Invalid or expired code.");
        }
    });
});

// 7. HISTORY ROUTE
app.get('/my-history', (req, res) => {
    if (!req.session.userId) return res.status(401).send("Unauthorized");
    db.all(`SELECT filename, action, file_password, timestamp FROM history WHERE user_id = ? ORDER BY timestamp DESC`, 
    [req.session.userId], (err, rows) => {
        if (err) return res.status(500).send("DB Error");
        res.json(rows);
    });
});

// 8. CORE ENGINE ROUTES (COMPRESS/DECOMPRESS)
app.post('/compress', upload.array('files'), async (req, res) => {
    if (!req.files || req.files.length === 0) return res.status(400).send("No files.");
    
    const password = req.body.password || "";
    const format = req.body.format || "zip";
    const isFolder = req.files.length > 1 || !!req.body.folderName;

    let downloadName = isFolder ? `${req.body.folderName || "archive"}_compressed.${format}` 
                               : `${path.parse(req.files[0].originalname).name}.${format}`;

    const tarPath = path.join('uploads', `bundle_${Date.now()}.tar`);
    const output = fs.createWriteStream(tarPath);
    const archive = archiver('tar');
    archive.pipe(output);
    
    req.files.forEach(file => archive.append(fs.createReadStream(file.path), { name: file.originalname }));
    
    output.on('close', () => {
        const outputEncrypted = `${tarPath}.${format}`;
        // Trigger C++ Engine
        // Trigger C++ Engine using the smart variable
        // Call C++ Engine using the smart variable with 5 second timeout safety
    
        exec(`${engineCmd} -c "${tarPath}" "${outputEncrypted}" "${password}"`, (err) => {

            if (err) { cleanup([tarPath], []); return res.status(500).send("C++ Error"); }
            
            // --- SECURITY CHECK: Only save if user is logged in ---
            if (req.session && req.session.userId) {
                db.run(`INSERT INTO history (user_id, filename, action, file_password) VALUES (?, ?, 'Compress', ?)`, 
                [req.session.userId, downloadName, password]);
            }
            // -------------------------------------------------------

            res.header("Access-Control-Expose-Headers", "Content-Disposition");
            res.download(outputEncrypted, downloadName, () => cleanup([tarPath, outputEncrypted, ...req.files.map(f => f.path)]));
        });
    });
    await archive.finalize();
});

app.post('/decompress', upload.single('files'), (req, res) => {
    if (!req.file) return res.status(400).send("No file.");
    
    const zipPath = path.join('uploads', `temp_${Date.now()}_${req.file.originalname}`);
    const tarPath = path.join('uploads', `restored_${Date.now()}.tar`);
    fs.renameSync(req.file.path, zipPath);

    // Call C++ Engine with 5 second timeout safety
    exec(`${engineCmd} -d "${zipPath}" "${tarPath}" "${req.body.password || ""}"`, { timeout: 5000 }, (err) => {
        if (err) { cleanup([zipPath, tarPath]); return res.status(401).send("Wrong Password or Engine Error"); }
        
        const extractDir = path.join('uploads', `extracted_${Date.now()}`);
        fs.mkdirSync(extractDir);
        
        try {
            tar.x({ file: tarPath, cwd: extractDir, sync: true });
            const items = fs.readdirSync(extractDir);
            
            if (items.length === 1 && fs.statSync(path.join(extractDir, items[0])).isFile()) {
                res.download(path.join(extractDir, items[0]), items[0], () => cleanup([zipPath, tarPath], [extractDir]));
            } else {
                const finalZip = extractDir + ".zip";
                const arch = archiver('zip');
                const output = fs.createWriteStream(finalZip);
                arch.pipe(output);
                arch.directory(extractDir, false);
                output.on('close', () => res.download(finalZip, "restored_files.zip", () => cleanup([zipPath, tarPath, finalZip], [extractDir])));
                arch.finalize();
            }
        } catch (e) { 
            res.status(500).send("Extraction Error"); 
            cleanup([zipPath, tarPath], [extractDir]); 
        }
    });
});
app.listen(port, () => console.log(`🚀 Server: http://localhost:${port}`));
const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('./database.db');

db.serialize(() => {
    console.log("Starting migration...");

    // Adding otp_code column
    db.run(`ALTER TABLE users ADD COLUMN otp_code TEXT`, (err) => {
        if (err) {
            console.log("otp_code column might already exist or error occurred.");
        } else {
            console.log("Added column: otp_code");
        }
    });

    // Adding otp_expiry column
    db.run(`ALTER TABLE users ADD COLUMN otp_expiry DATETIME`, (err) => {
        if (err) {
            console.log("otp_expiry column might already exist or error occurred.");
        } else {
            console.log("Added column: otp_expiry");
        }
    });
});

db.close();
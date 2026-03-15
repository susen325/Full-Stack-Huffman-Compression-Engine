# 🗜️ Full-Stack Huffman Compression Engine

A full-stack web application that securely compresses and decompresses files using a custom-built, native C++ Huffman Coding engine. The application features a Node.js/Express backend, secure user authentication, SQL-based activity tracking, and email-based OTP verification.

Currently deployed live on the cloud using Railway.

## ✨ Features

- **Native C++ Engine:** Utilizes a custom C++ algorithm implementing Huffman trees for lossless data compression, executed dynamically via the Node.js backend.
- **Secure Authentication:** User registration and login protected by `bcrypt` password hashing.
- **Email OTP Verification:** Integrated `Nodemailer` to handle secure password resets and 2FA-style dashboard unlocks via email.
- **User Dashboard:** A dynamically rendered SQLite3 history log tracking every file compressed, complete with a secure UI to view file-locking passwords.
- **Cross-Platform Cloud Deployment:** Containerized and configured to run native Linux binaries on Railway cloud infrastructure.

## 🛠️ Tech Stack

- **Core Engine:** C++ (File I/O, Priority Queues, Binary Tree Traversal)
- **Backend:** Node.js, Express.js
- **Database:** SQLite3
- **Frontend:** HTML5, CSS3, Vanilla JavaScript
- **Security/Auth:** bcrypt, express-session, Nodemailer
- **Deployment:** Railway (Ubuntu/Linux environment)

## 🚀 Running Locally

To run this project on your local machine, you will need **Node.js** and a **C++ Compiler (g++)** installed.

**1. Clone the repository:**
`git clone https://github.com/susen325/Full-Stack-Huffman-Compression-Engine.git`
`cd Full-Stack-Huffman-Compression-Engine`

**2. Install Node dependencies:**
`npm install`

**3. Configure Environment Variables:**
Create a `.env` file in the root directory and add the following:
`SESSION_SECRET=your_super_secret_string`
`GMAIL_USER=your_email@gmail.com`
`GMAIL_PASS=your_16_letter_app_password`

**4. Compile the Engine & Start the Server:**
`npm run build`
`npm start`
_The server will start on http://localhost:3000._

## 📈 Performance Notes

This engine uses Huffman Coding, which excels at finding repeating patterns in raw text.

- **Highly Compressible:** `.txt`, `.csv`, `.log`, and uncompiled code files will see massive size reductions.
- **Uncompressible:** Files that are already mathematically compressed (like `.pdf`, `.jpg`, or `.mp4`) will not shrink, and may slightly increase in size due to the Huffman tree metadata being attached to the file header.

## 🙏 Acknowledgments

Building a native C++ executable that communicates flawlessly with a cloud-deployed Node.js backend was a massive undertaking. I want to extend a massive thank you to:

*Abhiraj Singh:Thank you for the support, suggestions that kept this project moving forward.
*Gemini and Chatgpt: For acting as a pair-programmer, helping to untangle Linux cloud permissions, navigating Express server crashes, and making the deployment process a seamless learning experience.

---

_Developed by Susen Kumar._

const express = require('express');
const multer = require('multer');
const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');
const archiver = require('archiver');
const tar = require('tar');

const app = express();
const port = 3000;

const upload = multer({ dest: 'uploads/' });
if (!fs.existsSync('uploads')) fs.mkdirSync('uploads');

app.get('/', (req, res) => res.sendFile(path.join(__dirname, 'index.html')));

// --- 1. COMPRESS ---
app.post('/compress', upload.array('files'), async (req, res) => {
    if (!req.files || req.files.length === 0) return res.status(400).send("No files uploaded.");

    const password = req.body.password || "";
    const format = req.body.format || "zip"; // 🌟 GRAB FORMAT (.zip or .huff)
    
    const hasSlashes = /[/\\]/.test(req.files[0].originalname);
    const hasMultipleFiles = req.files.length > 1;
    const hasFolderNameField = !!req.body.folderName;
    const isFolder = hasMultipleFiles || hasSlashes || hasFolderNameField;

    let downloadName;

    if (isFolder) {
        let folderName = "archive";
        if (hasFolderNameField) folderName = req.body.folderName;
        else if (hasSlashes) folderName = req.files[0].originalname.split(/[/\\]/)[0];
        downloadName = `${folderName}_compressed.${format}`; // 🌟 APPLY FORMAT
    } else {
        let fileName = path.parse(req.files[0].originalname).name;
        downloadName = `${fileName}.${format}`; // 🌟 APPLY FORMAT
    }

    const tarPath = path.join('uploads', `bundle_${Date.now()}.tar`);
    const output = fs.createWriteStream(tarPath);
    const archive = archiver('tar');
    
    archive.pipe(output);
    req.files.forEach(file => {
        const safePathName = file.originalname.replace(/\\/g, '/');
        archive.append(fs.createReadStream(file.path), { name: safePathName });
    });
    
    output.on('close', () => {
        const outputEncrypted = `${tarPath}.${format}`; // 🌟 APPLY FORMAT TO OUTPUT
        
        // 🌟 Absolute path fallback just in case
        const exePath = path.join(__dirname, 'huffman.exe');
        const cmd = `"${exePath}" -c "${tarPath}" "${outputEncrypted}" "${password}"`;

        exec(cmd, (err) => {
            if (err) return res.status(500).send("C++ Compression Error");
            
            res.download(outputEncrypted, downloadName, () => {
                cleanup([tarPath, outputEncrypted, ...req.files.map(f => f.path)], []);
            });
        });
    });

    await archive.finalize();
});

// --- 2. DECOMPRESS ---
app.post('/decompress', upload.single('files'), (req, res) => {
    if (!req.file) return res.status(400).send("No file uploaded.");

    const zipPath = path.join('uploads', req.file.originalname);
    const tarPath = path.join('uploads', `restored_${Date.now()}.tar`);
    const password = req.body.password || "";

    fs.renameSync(req.file.path, zipPath);

    const exePath = path.join(__dirname, 'huffman.exe');
    const cmd = `"${exePath}" -d "${zipPath}" "${tarPath}" "${password}"`;
    
    exec(cmd, (err) => {
        if (err) return res.status(500).send("Decompression Error: Check Password.");

        const extractDir = path.join('uploads', `extracted_${Date.now()}`);
        fs.mkdirSync(extractDir);
        
        try {
            tar.x({ file: tarPath, cwd: extractDir, sync: true });
            
            const items = fs.readdirSync(extractDir);
            if (items.length === 0) throw new Error("Empty extracted folder");

            const firstItemPath = path.join(extractDir, items[0]);
            
            if (items.length === 1 && fs.statSync(firstItemPath).isFile()) {
                res.download(firstItemPath, items[0], () => {
                    cleanup([zipPath, tarPath], [extractDir]);
                });
            } else {
                const finalZip = extractDir + ".zip";
                const output = fs.createWriteStream(finalZip);
                const archive = archiver('zip');
                
                archive.pipe(output);
                archive.directory(extractDir, false);
                
                let finalDownloadName = req.file.originalname.replace('_compressed', '');
                
                // Fix names if restoring from .huff
                finalDownloadName = finalDownloadName.replace('.huff', '.zip');
                if (!finalDownloadName.endsWith('.zip')) {
                    finalDownloadName += '.zip';
                }

                output.on('close', () => {
                    res.download(finalZip, finalDownloadName, () => {
                        cleanup([zipPath, tarPath, finalZip], [extractDir]);
                    });
                });
                archive.finalize();
            }
        } catch (e) { 
            let fallbackName = req.file.originalname.replace('_compressed', '').replace('.zip', '').replace('.huff', '');
            res.download(tarPath, fallbackName, () => {
                cleanup([zipPath, tarPath], [extractDir]);
            });
        }
    });
});

function cleanup(files = [], dirs = []) {
    files.forEach(f => { if (fs.existsSync(f)) fs.unlinkSync(f); });
    dirs.forEach(d => { if (fs.existsSync(d)) fs.rmSync(d, { recursive: true, force: true }); });
}

app.listen(port, () => console.log(`🚀 Huffman Engine: http://localhost:${port}`));
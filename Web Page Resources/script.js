// === Status Update ===
function updateStatus() {
  fetch('/status')
    .then(r => r.json())
    .then(s => {
      let txt = `🌐 ${s.ip} | 📶 ${s.rssi}dBm | 💾 ${s.sd_used_mb}MB Used`;
      const box = document.getElementById('statusBox');
      if (box) box.innerText = txt;
    });
}
setInterval(updateStatus, 3000);
updateStatus();


// === Upload Handler ===
if (document.getElementById('uploadForm')) {
  document.getElementById('uploadForm').onsubmit = function (e) {
    e.preventDefault();
    let file = document.getElementById('fileInput').files[0];
    if (!file) return;

    let xhr = new XMLHttpRequest();
    xhr.open("POST", "/upload", true);
    xhr.upload.onprogress = function (e) {
      if (e.lengthComputable) {
        let p = (e.loaded / e.total) * 100;
        document.getElementById('bar').style.width = p + '%';
      }
    };
    xhr.onload = function () {
      document.getElementById('bar').style.width = '0%';
      loadFiles(); // refresh after upload
    };
    let f = new FormData();
    f.append("data", file);
    xhr.send(f);
  };
}


// === File Loader ===
async function loadFiles() {
  try {
    const res = await fetch('/list');
    const files = await res.json();
    const tbody = document.getElementById('fileList') || document.getElementById('filelist');
    if (!tbody) return;

    // protected file list
    const protectedFiles = [
      'index.html', 'about.html', 'files.html', 'system.html',
      'style.css', 'script.js', 'config.txt', 'server.key'
    ];

    // যদি পুরোনো div layout হয়
    if (tbody.tagName.toLowerCase() === 'div') {
      let html = '<table><tr><th>File</th><th>Size</th><th>Action</th></tr>';
      files.forEach(f => {
        let delBtn = protectedFiles.includes(f.name)
          ? ''
          : `<button onclick="deleteFile('${encodeURIComponent(f.name)}')">🗑 Delete</button>`;
        html += `<tr>
          <td><a href="/download?name=${encodeURIComponent(f.name)}">${f.name}</a></td>
          <td>${(f.size / 1024).toFixed(2)} KB</td>
          <td>${delBtn}</td>
        </tr>`;
      });
      html += '</table>';
      tbody.innerHTML = html;
    } else {
      // নতুন টেবিল (tbody) layout
      tbody.innerHTML = '';
      files.forEach(f => {
        const tr = document.createElement('tr');
        const icon =
          f.name.endsWith('.jpg') || f.name.endsWith('.jpeg') || f.name.endsWith('.png') || f.name.endsWith('.gif') ? '🖼️' :
          f.name.endsWith('.mp4') || f.name.endsWith('.webm') ? '🎞️' :
          f.name.endsWith('.pdf') ? '📄' :
          f.name.endsWith('.txt') ? '📝' :
          '📁';

        const isProtected = protectedFiles.includes(f.name);
        tr.innerHTML = `
          <td>${icon}</td>
          <td>${f.name}</td>
          <td>${(f.size / 1024).toFixed(1)} KB</td>
          <td>
            <button class="action-btn" title="Preview" onclick="previewFile('${f.name}')">👁️</button>
            <button class="action-btn" title="Download" onclick="downloadFile('${f.name}')">🔽</button>
            ${!isProtected ? `<button class="action-btn" title="Delete" onclick="deleteFile('${f.name}')">🗑️</button>` : ''}
          </td>`;
        tbody.appendChild(tr);
      });
    }
  } catch (e) {
    console.error('File list error:', e);
  }
}


// === File Delete ===
function deleteFile(name) {
  if (confirm(`Delete ${name}?`)) {
    fetch(`/delete?name=${encodeURIComponent(name)}`).then(r => {
      if (r.ok) loadFiles();
      else alert('❌ Delete failed');
    });
  }
}


// === File Download ===
function downloadFile(name) {
  window.location = `/download?name=${encodeURIComponent(name)}`;
}


// === File Preview ===
function previewFile(name) {
  const ext = name.split('.').pop().toLowerCase();
  const box = document.getElementById('previewBox');
  const modal = document.getElementById('previewModal');
  if (!box || !modal) {
    // fallback
    window.open(`/download?name=${encodeURIComponent(name)}`, '_blank');
    return;
  }

  box.innerHTML = '';

  if (['jpg', 'jpeg', 'png', 'gif'].includes(ext)) {
    box.innerHTML = `<img id="previewImg" src="/download?name=${name}" alt="${name}">`;
  } else if (['mp4', 'webm'].includes(ext)) {
    box.innerHTML = `<video id="previewVideo" src="/download?name=${name}" controls autoplay></video>`;
  } else if (ext === 'pdf') {
    box.innerHTML = `<iframe id="previewFrame" src="/download?name=${name}" frameborder="0"></iframe>`;
  } else if (ext === 'txt') {
    fetch(`/download?name=${name}`)
      .then(r => r.text())
      .then(text => {
        box.innerHTML = `<pre style="color:white; text-align:left;">${text}</pre>`;
      });
  } else {
    box.innerHTML = `<p style="color:white;">🔍 Preview not supported for this file type.</p>`;
  }

  modal.style.display = 'flex';
}


// === Close Preview ===
function closePreview() {
  const modal = document.getElementById('previewModal');
  const box = document.getElementById('previewBox');
  if (modal && box) {
    modal.style.display = 'none';
    box.innerHTML = '';
  }
}

// === Auto Refresh ===
window.onload = () => {
  loadFiles();
  setInterval(loadFiles, 5000);
};

// === YouTube Gallery Auto Loader ===
document.addEventListener('DOMContentLoaded', () => {
  // 🔹 তোমার YouTube ভিডিও লিংকগুলো এখানে বসাও
  const ytLinks = [
    "https://youtu.be/I7M--UzFerM",
    "https://youtu.be/pcqFiv7YSKM",
    "https://youtu.be/YHOgd6AzgVY",
    "https://youtu.be/5-08KZvjgXQ"
  ];

  const gallery = document.getElementById("ytGallery");
  if (!gallery) return;

  ytLinks.forEach(link => {
    const match = link.match(/(?:v=|youtu\.be\/)([a-zA-Z0-9_-]{11})/);
    if (!match) return;

    const videoId = match[1];
    const thumbUrl = `https://img.youtube.com/vi/${videoId}/hqdefault.jpg`;
    const videoUrl = `https://www.youtube.com/watch?v=${videoId}`;

    // Create card
    const card = document.createElement("div");
    card.className = "video-card";
    card.innerHTML = `
      <a href="${videoUrl}" target="_blank">
        <img src="${thumbUrl}" alt="YouTube Video">
      </a>
      <h4>Loading...</h4>
    `;
    gallery.appendChild(card);

    // Fetch title automatically from YouTube
    fetch(`https://www.youtube.com/oembed?url=${videoUrl}&format=json`)
      .then(res => res.json())
      .then(data => {
        card.querySelector("h4").innerText = data.title;
      })
      .catch(() => {
        card.querySelector("h4").innerText = "🎞️ Unable to load title";
      });
  });
});

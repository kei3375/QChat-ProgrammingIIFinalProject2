// toggle active friend on click
document.querySelectorAll('.friend-item').forEach(item => {
  item.addEventListener('click', () => {
    document.querySelectorAll('.friend-item').forEach(i => i.classList.remove('active'));
    item.classList.add('active');
  });
});



let this_uuid = "$$uuid$$";
let friend_uuid = "$$selected uuid$$";
let friend_username = "$$selected user username$$";



let audioUnlocked = false;
const notificationAudio = new Audio('/notification');

document.addEventListener('click', () => {
  if (!audioUnlocked) {
    notificationAudio.play().then(() => {
      notificationAudio.pause();
      notificationAudio.currentTime = 0;
      audioUnlocked = true;
    }).catch(() => {});
  }
}, { once: false });



async function readAllMessages() {
  const response = await fetch(`/messages/${friend_uuid}`);
  const messages = await response.json();
  const area = document.querySelector('.messages-area');

  const temp = document.createElement('div');

  messages.forEach(msg => {
    const row = document.createElement('div');

    row.className = msg.sender === 'you' ? 'msg-row mine' : 'msg-row';
    row.innerHTML = `
      <div class="msg-avatar">${msg.sender === 'you' ? '<img src="/profile_picture.jpg" width="30" class="pfp">' : '<img src="/pfps/'+friend_uuid+'" width="30" class="pfp">'}</div>
      <div class="msg-bubble-group">
        <div class="msg-bubble">${msg.message.replace(/</g,'&lt;')}</div>
        <div class="msg-time">${msg.time.slice(0, 5)}</div>
        <div class="friend-meta">${msg.notifications>0?"<span class='unread-badge'>"+msg.notifications+"</span>":""}</div>
      </div>`;
    
    temp.prepend(row);
  });

  replace_tags_with_imgs_in(temp);

  const newHTML = temp.innerHTML;

  // Sprawdź czy w nowym HTML są jeszcze nierozwiązane tagi (nie powinno być po replace,
  // ale sprawdź czy obrazki w DOM już się załadowały przez weryfikację src)
  const currentImgs = area.querySelectorAll('img.chat-img');
  const newImgs = temp.querySelectorAll('img.chat-img');
  
  // Wymuś aktualizację jeśli: HTML się zmienił LUB liczba obrazków się zmieniła
  // const changed = area.innerHTML !== newHTML || currentImgs.length !== newImgs.length;
  // const changed = area.innerHTML !== newHTML || currentImgs.length !== newImgs.length || forceRefresh;
  // const changed = area.innerHTML !== newHTML || currentImgs.length !== newImgs.length || forceRefresh;
  const currentMessagesCount = area.querySelectorAll('.msg-row').length;
  const newMessagesCount = temp.querySelectorAll('.msg-row').length;
  const changed = currentMessagesCount !== newMessagesCount || forceRefresh;
  if (changed) {
    forceRefresh = false; // ← resetuj po użyciu
    const wasAtBottom = area.scrollTop + area.clientHeight >= area.scrollHeight - 10;
    area.innerHTML = newHTML;
    if (wasAtBottom) area.scrollTop = area.scrollHeight;
    loadFriendList();
    watchUnloadedImages(); // ← dodaj tutaj
    if (aktywneWyszukiwanie !== '') zastosujFiltr();
  }
}


async function exchange_keys() {
  const res = await fetch("/exchange_keys/" + friend_uuid);
  setTimeout(function(){select_user(friend_uuid, friend_username)},100);
  setTimeout(function(){select_user(friend_uuid, friend_username)},500);
}

async function send_message(uuid, message) {
  await fetch("/send/" + uuid, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ message: message })
  });

  loadFriendList();
}

// function _send_message() {
//   // alert(1)
//   const area = document.querySelector('.messages-area');
//     send_message(friend_uuid, document.querySelector("#input-text").value);
//     readAllMessages();
//     area.scrollTop = area.scrollHeight;
//     document.querySelector("#input-text").value = '';
// }

// async function _send_message() {
//     const area = document.querySelector('.messages-area');
//     const text = document.querySelector("#input-text").value.trim();

//     if (selectedFile) {
//       await fetch('/upload/file/' + friend_uuid, {
//           method: 'POST',
//           headers: { 'X-Filename': selectedFile.name },
//           body: selectedFile
//       });
//       clearAttachment(); // ← dodaj tutaj
//       await fetch("/send/" + friend_uuid, {
//         method: "POST",
//         headers: { "Content-Type": "application/json" },
//         // body: JSON.stringify({ message: "<wysłano plik>" })
        
//           // body: JSON.stringify({ message: "<<"+selectedFile.name+">>" })
//           body: (selectedFile.name.endsWith(".jpg") 
//             ? JSON.stringify({ message: "<<"+selectedFile.name+">>" }) 
//             : JSON.stringify({ message: "<wysłano plik>" }))
//       });
//     }

//     if (!text) return;

//     send_message(friend_uuid, text);
//     readAllMessages();
//     area.scrollTop = area.scrollHeight;
//     document.querySelector("#input-text").value = '';
// }

async function _send_message() {
    const area = document.querySelector('.messages-area');
    const text = document.querySelector("#input-text").value.trim();

    if (selectedFile) {
      const fileName = selectedFile.name;
      await fetch('/upload/file/' + friend_uuid, {
          method: 'POST',
          headers: { 'X-Filename': fileName },
          body: selectedFile
      });
      clearAttachment();
      await fetch("/send/" + friend_uuid, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ message: fileName.endsWith(".jpg") ? "<<"+fileName+">>" : "<wysłano plik>" })
      });

      // odśwież kilka razy z rosnącym opóźnieniem
      for (const delay of [100, 300, 700, 1500]) {
        await new Promise(r => setTimeout(r, delay));
        await readAllMessages();
      }
      return;
    }

    if (!text) return;
    send_message(friend_uuid, text);
    readAllMessages();
    area.scrollTop = area.scrollHeight;
    document.querySelector("#input-text").value = '';
}

// send on Enter
document.getElementById('chat-input').addEventListener('keydown', e => {
  if (e.key === 'Enter' && e.target.value.trim()) {
    const area = document.querySelector('.messages-area');
    send_message(friend_uuid, e.target.value);
    readAllMessages();
    area.scrollTop = area.scrollHeight;
    e.target.value = '';
  }
});

// send button
document.querySelector('.send-btn').addEventListener('click', () => {
  const input = document.querySelector('#chat-input');
  if (input.value.trim()) {
    input.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  }
});


// let friend_username = "$$selected user username$$";


async function select_user(uuid, username) {
  await fetch("/select_user/"+uuid);

  const ip_data_json = await fetch("/ip_data/"+uuid);

  const key_status = await fetch("/key_status/"+uuid);

  const stat = await key_status.text();
  // alert(stat);

  document.querySelector("#key-status").innerHTML = (stat === "true"?"key status: <br><text class='text-online'>exists</text>":"key status: <br><text class='text-warning'>no key</text>");

  const ip_data = await ip_data_json.json();

  // document.getElementById("options-ip").innerHTML = ip_data["ip"];
  // document.getElementById("options-iport").innerHTML = "iport:"+ip_data["iport"];
  // document.getElementById("options-oport").innerHTML = "oport:"+ip_data["oport"];
  document.querySelector("#options-ip span").textContent = ip_data["ip"];
  document.querySelector("#options-iport span").textContent = "iport:" + ip_data["iport"];
  document.querySelector("#options-oport span").textContent = "oport:" + ip_data["oport"];

  // alert(ip_data["ip"]);

  // alert(1);
  friend_uuid = uuid;

  
  readAllMessages();
  loadFriendList();
  document.getElementById("chat-header-pfp").innerHTML = `<img src="pfps/${uuid}" width="40"></img>`;
  document.getElementById("chat-header-username").innerHTML = username;
}


let friends_uuids = [];

async function loadFriendList() {
  const response = await fetch(`/friend_list`);
  const friends = await response.json();

  const area = document.querySelector('.friend-list');
  json_friend_list = response;

  friends.sort((a, b) => {
    const dateA = new Date((a.last_message_date || "0") + " " + (a.last_message_time || "0"));
    const dateB = new Date((b.last_message_date || "0") + " " + (b.last_message_time || "0"));
    return dateB - dateA;
  });

  friends_uuids = [];

  const temp = document.createElement('div');

  friends.forEach(usr => {
    const row = document.createElement('div');
    row.className = usr.selected === 'true' ? 'friend-item active' : 'friend-item';
    
    friends_uuids.push(usr.uuid);
    row.onclick = () => select_user(usr.uuid, usr.username);

    row.innerHTML = `
      <div class="friend-avatar pfp" style="background-image: url('/pfps/${usr.uuid}');" id="activity:${usr.uuid}">
          <span class="status-dot ${usr.status}"></span>
      </div>
      <div class="friend-info">
          <div class="friend-name">${usr.username}</div>
          <div class="friend-preview">${usr.last_message}</div>
      </div>
      <div class="friend-meta">
          ${usr.notifications!='0'?'<span class="unread-badge">'+ usr.notifications + '</span>':''}
      </div>`;
    temp.appendChild(row);
  });

  // aktualizuj DOM tylko jeśli coś się zmieniło
  if (area.innerHTML !== temp.innerHTML) {
    
    // sprawdź czy pojawiło się nowe powiadomienie
    const oldBadges = [...area.querySelectorAll('.unread-badge')].map(b => b.textContent).join(',');
    const newBadges = [...temp.querySelectorAll('.unread-badge')].map(b => b.textContent).join(',');
    if (newBadges !== oldBadges) {
      notificationAudio.currentTime = 0;
      notificationAudio.play().catch(() => {});
      console.log("notification");
    }

    area.innerHTML = temp.innerHTML;
    area.querySelectorAll('.friend-item').forEach((row, i) => {
      const usr = friends[i];
      row.onclick = () => select_user(usr.uuid, usr.username);
    });
  }
}


//load everything

// let json_friend_list="";

loadFriendList();
readAllMessages();
checkActivity();
select_user("$$selected uuid$$", "$$selected user username$$");

async function checkActivity () {
  friends_uuids.forEach(async function(val){
    const res = await fetch("/check_activity/"+val);
    const text = await res.text();


    // console.log("checking activity...");

    const activity = document.getElementById("activity:"+val);
    
      // Znajdź i zmień tylko span:
      const dot = activity.querySelector('.status-dot');
      if (dot) {
          dot.className = text === "true" ? 'status-dot online' : 'status-dot offline';
          if(val == friend_uuid) {
            const status = document.getElementById("activity");
            if( text ==="true" ) {
              status.innerHTML = "online now";
              status.classList.remove("offline");
              status.classList.add("online");
            }
            else {
              status.innerHTML = "offline now";
              status.classList.remove("online");
              status.classList.add("offline");
            }
          }
      }

    
  });
}

//ustawienia
          function openSettings() {
              const modal = document.getElementById('settings-modal');
              modal.style.display = 'flex';
          }

          async function saveName() {
              const name = document.getElementById('settings-name-input').value.trim();
              if (!name) return;
              await fetch('/upload/name', {
                  method: 'POST',
                  headers: { 'Content-Type': 'application/json' },
                  body: JSON.stringify({ name })
              });
              document.querySelector('.my-name').textContent = name;
          }

          async function savePfp(file) {
              const buf = await file.arrayBuffer();
              await fetch('/upload/pfp', {
                  method: 'POST',
                  headers: { 'Content-Type': 'image/jpeg' },
                  body: buf
              });
              // odśwież obrazek (cache-bust)
              document.querySelector('.my-avatar').style.backgroundImage = 
                  `url('/profile_picture.jpg?t=${Date.now()}')`;
          }

          document.querySelector('.settings-btn').addEventListener('click', openSettings);

          document.getElementById('settings-pfp-input')
              .addEventListener('change', e => savePfp(e.target.files[0]));
          document.getElementById('settings-name-save')
              .addEventListener('click', saveName);
          document.getElementById('settings-close')
              .addEventListener('click', () => {
                  document.getElementById('settings-modal').style.display = 'none';
              });
//ustawienia;

//ustawienia ip
          async function editField(field, el) {
            // nie otwieraj drugiego inputa jeśli już jest
            if (el.querySelector('input')) return;

            const span = el.querySelector('span');
            const oldVal = span.textContent.replace(/^(iport:|oport:)/, '').trim();

            const input = document.createElement('input');
            input.value = oldVal;
            // input.style.cssText = 'background:none;border:none;outline:none;color:var(--accent);font-family:inherit;font-size:inherit;width:80px;';
            input.style.cssText = 'background:none;border:none;outline:none;color:var(--accent);font-family:inherit;font-size:inherit;flex:1;min-width:0;width:100%;';

            span.replaceWith(input);
            input.focus();
            input.select();

            async function confirm() {
              const newVal = input.value.trim();
              if (!newVal || newVal === oldVal) {
                // przywróć bez zmian
                const s = document.createElement('span');
                s.textContent = field === 'ip' ? newVal || oldVal : field + ':' + (newVal || oldVal);
                input.replaceWith(s);
                return;
              }

              await fetch('/update_ip_data/' + friend_uuid, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ field, value: newVal })
              });

              const s = document.createElement('span');
              s.textContent = field === 'ip' ? newVal : field + ':' + newVal;
              input.replaceWith(s);
            }

            input.addEventListener('keydown', e => {
              if (e.key === 'Enter') confirm();
              if (e.key === 'Escape') {
                const s = document.createElement('span');
                s.textContent = field === 'ip' ? oldVal : field + ':' + oldVal;
                input.replaceWith(s);
              }
            });
            input.addEventListener('blur', confirm);
          }
//ustawienia ip;



//dodawanie znajomych
          // ── DODAWANIE / USUWANIE ZNAJOMYCH ──

          function isValidUUID(str) {
            // return /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(str.trim());
            // return ;
            const regex = /^[a-z0-9-]{36}$/;
            return regex.test(str);
          }

          function openAddFriendModal() {
            // alert("add friend modal");
            document.getElementById('add-friend-uuid-input').value = '';
            document.getElementById('add-friend-uuid-input').style.borderColor = 'var(--border)';
            document.getElementById('add-friend-error').style.display = 'none';
            document.getElementById('add-friend-modal').style.display = 'flex';
          }

          function closeAddFriendModal() {
            document.getElementById('add-friend-modal').style.display = 'none';
          }

          async function confirmAddFriend() {
            const input = document.getElementById('add-friend-uuid-input');
            const uuid_to_add = input.value.trim();
            const errEl = document.getElementById('add-friend-error');

            if (!isValidUUID(uuid_to_add)) {
              errEl.style.display = 'block';
              errEl.textContent = 'Nieprawidłowy UUID.';
              return;
            }

            const res = await fetch('/add_friend/' + uuid_to_add, { method: 'POST' });
            if (!res.ok) {
              errEl.style.display = 'block';
              errEl.textContent = 'Błąd serwera.';
              return;
            }

            closeAddFriendModal();
            loadFriendList();
          }

          async function removeCurrentFriend() {
            if (!friend_uuid) return;
            if (!confirm('Usunąć znajomego?')) return;

            await fetch('/remove_friend/' + friend_uuid, { method: 'POST' });

            friend_uuid = '';
            loadFriendList();
            document.querySelector('.messages-area').innerHTML = '';
            document.getElementById('chat-header-username').innerHTML = '';
            document.getElementById('chat-header-pfp').innerHTML = '';
          }

          // zamknij modal po kliknięciu tła
          document.getElementById('add-friend-modal').addEventListener('click', function(e) {
            if (e.target === this) closeAddFriendModal();
          });

          // zamknij modal Escape
          document.addEventListener('keydown', e => {
            if (e.key === 'Escape') closeAddFriendModal();
          });
//dodawanie znajomych;


//dodawania plików
          let selectedFile = null;

          function onFileSelected(input) {
              const file = input.files[0];
              if (!file) return;
              selectedFile = file;

              const preview = document.getElementById('attachment-preview');
              const nameEl = document.getElementById('attachment-name');
              nameEl.textContent = file.name;
              nameEl.title = file.name;
              preview.style.display = 'flex';

              // zresetuj input żeby można było wybrać ten sam plik ponownie
              input.value = '';
          }

          function clearAttachment() {
              selectedFile = null;
              document.getElementById('attachment-preview').style.display = 'none';
              document.getElementById('attachment-name').textContent = '';
          }

//dodawania plików;

function replace_tags_with_imgs_in(container) {
  container.querySelectorAll('.msg-bubble').forEach(bubble => {
    const text = bubble.textContent.trim();
    const match = text.match(/^<<([a-zA-Z0-9_\-().]{1,200}\.jpg)>>$/);
    
    if (match) {
      const filename = match[1];
      const isMine = bubble.closest('.msg-row').classList.contains('mine');
      const uuid = isMine ? this_uuid : friend_uuid;
      bubble.style.width = '70%';
      
      bubble.innerHTML = '';
      const img = document.createElement('img');
      img.className = 'chat-img';
      img.style.width = '100%';
      
      const parts = uuid + '/' + encodeURIComponent(filename);
      
      // Jeśli mamy timestamp dla tego zdjęcia (czyli właśnie przyszło nowe), używamy go
      if (imageTimestamps[parts]) {
        img.src = '/imgs/' + parts + '?t=' + imageTimestamps[parts];
      } else {
        img.src = '/imgs/' + parts;
      }
      
      bubble.appendChild(img);
    }
  });
}

// stara wersja dla kompatybilności
function replace_tags_with_imgs() {
  replace_tags_with_imgs_in(document.querySelector('.messages-area'));
}

let forceRefresh = false;

const appStartTime = Date.now(); // Zapamiętuje moment otwarcia strony
const imageTimestamps = {};      // Przechowuje unikalne identyfikatory dla nowych zdjęć
const confirmedImages = new Set();
const watchingImages = new Set();

function watchUnloadedImages() {
  const area = document.querySelector('.messages-area');
  
  area.querySelectorAll('img.chat-img').forEach(img => {
    const src = img.src.split('?')[0];
    const parts = src.split('/imgs/')[1];
    if (!parts) return;
    
    if (confirmedImages.has(parts)) return;
    if (watchingImages.has(parts)) return;
    
    watchingImages.add(parts);
    let attempts = 0;
    
    const retry = setInterval(async () => {
      try {
        attempts++;
        const res = await fetch('/img_exists/' + parts);
        const text = await res.text();
        
        if (text === 'true') {
          clearInterval(retry);
          watchingImages.delete(parts);
          confirmedImages.add(parts);
          
          // Jeśli strona niedawno się uruchomiła i sprawdzamy historię czatu,
          // zostawiamy stare zdjęcia w spokoju (nie wymuszamy re-renderowania HTML).
          if (attempts === 1 && (Date.now() - appStartTime < 3000)) {
            return; 
          }
          
          // Jeśli to nowe zdjęcie wysłane/odebrane w trakcie rozmowy:
          // Generujemy nowy timestamp i wymuszamy odświeżenie HTML.
          imageTimestamps[parts] = Date.now();
          forceRefresh = true;
        }
      } catch (e) {}
    }, 1000);
  });
}



let server_connection = "$$server connection$$";


async function switch_connection() {
  const btn = document.getElementById("server-color");
  
  // Pobieramy aktualny stan bezpośrednio z drzewa DOM (z tekstu w środku)
  const currentVisualState = btn.textContent.trim(); 
  const newState = currentVisualState === "off" ? "on" : "off";
  
  // Wysyłamy informację do serwera
  await fetch("/connection", {
    method: "POST",
    headers: { "Content-Type": "text/plain" },
    body: newState
  });
  
  // Aktualizujemy zmienną globalną (jeśli jest potrzebna w innych miejscach)
  server_connection = newState;
  
  // Aktualizujemy wygląd w HTML
  if (newState === "on") {
    btn.textContent = "on";
    btn.classList.add("text-online");
  } else {
    btn.textContent = "off";
    btn.classList.remove("text-online");
  }
}

// Na samym dole pliku, obok innych wywołań startowych:
function initServerButton() {
  const btn = document.getElementById("server-color");
  // Wstrzykujemy wartość z serwera bezpośrednio do tekstu na starcie
  btn.textContent = server_connection; 
  
  if (server_connection === "on") {
    btn.classList.add("text-online");
  } else {
    btn.classList.remove("text-online");
  }
}

// Wywołanie przy starcie
initServerButton();


async function loadMyInfo() {
  try {
    const res = await fetch("/my_info");
    const data = await res.json();
    document.getElementById("my-public-ip").textContent = data.public_ip || "—";
    document.getElementById("my-local-ip").textContent  = data.local_ip  || "—";
    document.getElementById("my-c-uuid").textContent    = data.c_uuid    || "nie zarejestrowano";
    
    // FIX: Update this_uuid if it was empty on load
    if (this_uuid === "" && data.c_uuid && data.c_uuid !== "nie zarejestrowano") {
        this_uuid = data.c_uuid;
    }
  } catch (e) {}
}


setInterval(function(){
  readAllMessages();
  checkActivity();
  loadFriendList();
  loadMyInfo();
  // console.log(friend_uuid);
},
// 125
300
);

// const keepAlive = setInterval(function(){   //send a request every 15 seconds to keep local server alive (it shuts down after 30s of inactivity), currently not needed because other intervals send requests more frequently all the time
//   await fetch("/");
// },15000);


// ===================================================
// ALGORYTM LEVENSHTEINA
// ===================================================
function liczLevenshteina(slowoA, slowoB) {
    if (slowoA.length === 0) return slowoB.length;
    if (slowoB.length === 0) return slowoA.length;

    const macierz = [];

    for (let i = 0; i <= slowoB.length; i++) macierz[i] = [i];
    for (let j = 0; j <= slowoA.length; j++) macierz[0][j] = j;

    for (let i = 1; i <= slowoB.length; i++) {
        for (let j = 1; j <= slowoA.length; j++) {
            if (slowoB.charAt(i - 1) === slowoA.charAt(j - 1)) {
                macierz[i][j] = macierz[i - 1][j - 1];
            } else {
                macierz[i][j] = Math.min(
                    macierz[i - 1][j - 1] + 1,
                    Math.min(
                        macierz[i][j - 1] + 1,
                        macierz[i - 1][j] + 1
                    )
                );
            }
        }
    }
    return macierz[slowoB.length][slowoA.length];
}

// ===================================================
// WYSZUKIWANIE — podpięcie pod pasek
// Przeniesione do DOMContentLoaded żeby mieć pewność
// że .topbar-search już istnieje w DOM w momencie
// wykonania querySelector (wcześniej zwracał null).
// ===================================================
document.addEventListener('DOMContentLoaded', function() {
  // const poleWyszukiwania = document.querySelector('.op-search input');
  const poleWyszukiwania = document.getElementById("op-search");
  if (!poleWyszukiwania) {alert(1);return;};

  poleWyszukiwania.addEventListener('keydown', function(zdarzenie) {
    if (zdarzenie.key === 'Enter') {
      // alert(1)
      aktywneWyszukiwanie = poleWyszukiwania.value.toLowerCase().trim();
      zastosujFiltr();
    }
  });

  // Reset gdy pole wyczyszczone
  poleWyszukiwania.addEventListener('input', function() {
    if (poleWyszukiwania.value === '') {
      aktywneWyszukiwanie = '';
      document.querySelectorAll('.msg-row').forEach(wiersz => {
        wiersz.style.display = '';
        wiersz.querySelectorAll('.msg-bubble').forEach(b => b.classList.remove('search-highlight'));
      });
    }
  });
});


// ===================================================
// WYSZUKIWANIE — podpięcie pod pasek
// Przeniesione do DOMContentLoaded żeby mieć pewność
// że .topbar-search już istnieje w DOM w momencie
// wykonania querySelector (wcześniej zwracał null).
// ===================================================
document.addEventListener('DOMContentLoaded', function() {
  const poleWyszukiwania = document.querySelector('.topbar-search input');
  if (!poleWyszukiwania) return;

  poleWyszukiwania.addEventListener('keydown', function(zdarzenie) {
    if (zdarzenie.key === 'Enter') {
      aktywneWyszukiwanie = poleWyszukiwania.value.toLowerCase().trim();
      zastosujFiltr();
    }
  });

  // Reset gdy pole wyczyszczone
  poleWyszukiwania.addEventListener('input', function() {
    if (poleWyszukiwania.value === '') {
      aktywneWyszukiwanie = '';
      document.querySelectorAll('.msg-row').forEach(wiersz => {
        wiersz.style.display = '';
        wiersz.querySelectorAll('.msg-bubble').forEach(b => b.classList.remove('search-highlight'));
      });
    }
  });
});

// ===================================================
// STAN WYSZUKIWANIA — globalna zmienna pamiętana
// między odświeżeniami readAllMessages()
// ===================================================
let aktywneWyszukiwanie = '';

function zastosujFiltr() {
  if (aktywneWyszukiwanie === '') return;

  document.querySelectorAll('.msg-row').forEach(function(wiersz) {
    const dymki = wiersz.querySelectorAll('.msg-bubble');
    let wierszPasuje = false;

    dymki.forEach(function(dymek) {
      dymek.classList.remove('search-highlight');
      const tekstDymka = dymek.textContent.toLowerCase();
      const slowaWiadomosci = tekstDymka.split(/\s+/);
      let dymekPasuje = false;

      for (let i = 0; i < slowaWiadomosci.length; i++) {
        if (liczLevenshteina(aktywneWyszukiwanie, slowaWiadomosci[i]) <= 2) {
          dymekPasuje = true;
          break;
        }
      }

      if (dymekPasuje) {
        dymek.classList.add('search-highlight');
        wierszPasuje = true;
      }
    });

    wiersz.style.display = wierszPasuje ? '' : 'none';
  });
}
// toggle active friend on click
document.querySelectorAll('.friend-item').forEach(item => {
  item.addEventListener('click', () => {
    document.querySelectorAll('.friend-item').forEach(i => i.classList.remove('active'));
    item.classList.add('active');
  });
});

const friend_uuid = "f47ac10b-58cc-4372-a567-0e02b2c3d479";

async function readAllMessages() {
  const response = await fetch(`/messages/${friend_uuid}`);
  const messages = await response.json();
  const area = document.querySelector('.messages-area');
  area.innerHTML = '';

  messages.forEach(msg => {
    const row = document.createElement('div');
    row.className = msg.sender === 'you' ? 'msg-row mine' : 'msg-row';
    row.innerHTML = `
      <div class="msg-avatar">${msg.sender === 'you' ? 'TY' : 'FR'}</div>
      <div class="msg-bubble-group">
        <div class="msg-bubble">${msg.message.replace(/</g,'&lt;')}</div>
        <div class="msg-time">${msg.time.slice(0, 5)}</div>
      </div>`;
    area.prepend(row);
  });

  area.scrollTop = area.scrollHeight;
}



async function send_message(uuid, message) {
  await fetch("/send/" + uuid, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ message: message })
  });
}


// send on Enter
document.querySelector('.input-wrap input').addEventListener('keydown', e => {
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
  const input = document.querySelector('.input-wrap input');
  if (input.value.trim()) {
    input.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  }
});





//everything loaded

readAllMessages();
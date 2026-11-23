async function refreshInfo() {
  try {
    const response = await fetch('/api/status', {cache: 'no-store'});
    if (!response.ok) throw new Error('Falha ao consultar status');
    const data = await response.json(), s = data.system || {};
    document.getElementById('host').textContent = s.hostname || 'N/A';
    document.getElementById('ip').textContent = s.ip_address || 'N/A';
    document.getElementById('cpu').textContent = Number.isFinite(s.cpu_percent) ? s.cpu_percent.toFixed(1) + '%' : 'N/A';
    document.getElementById('temp').textContent = Number.isFinite(s.temperature_c) ? s.temperature_c.toFixed(1) + ' °C' : 'N/A';
    document.getElementById('error').textContent = '';
  } catch(e) { document.getElementById('error').textContent = 'Não foi possível atualizar as métricas.'; }
}
refreshInfo();
setInterval(refreshInfo, 3000);
document.getElementById('date').textContent = new Date().toLocaleDateString('pt-BR');

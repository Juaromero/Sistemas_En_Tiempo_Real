var seconds = null;
var otaTimerVar = null;
var statusInterval = null;
var fanMode = 0;
var svMode = 1;

$(document).ready(function(){
  getUpdateStatus();
  loadInitialState();
  startStatusPoll();
  bindEvents();
});

function loadInitialState() {
  $.getJSON('/api/status', function(d) {
    syncFormFromServer(d);
  });
}

function bindEvents() {
  $("#fan-auto").on("click", function(){ setFanMode(0); });
  $("#fan-manual").on("click", function(){ setFanMode(1); });
  $("#fan-apply").on("click", applyFan);
  $("#fan-td").on("input", function(){ $("#fan-td-val").text(this.value); });
  $("#fan-tm").on("input", function(){ $("#fan-tm-val").text(this.value); });
  $("#fan-mp").on("input", function(){ $("#fan-mp-val").text(this.value); });

  $("#sv-auto").on("click", function(){ setSvMode(0); });
  $("#sv-manual").on("click", function(){ setSvMode(1); });
  $("#sv-apply").on("click", applyServo);
  $("#sv-mp").on("input", function(){ $("#sv-mp-val").text(this.value); });

  $("#rgb-r").on("input", function(){
    $("#rgb-r-val").text(this.value); updateRGBPreview();
  });
  $("#rgb-g").on("input", function(){
    $("#rgb-g-val").text(this.value); updateRGBPreview();
  });
  $("#rgb-b").on("input", function(){
    $("#rgb-b-val").text(this.value); updateRGBPreview();
  });
  $("#rgb-apply").on("click", applyRGB);
  $("#rgb-br").on("input", function(){ $("#rgb-br-val").text(this.value); });

  $("#wifi-sta-save").on("click", saveWiFiSTA);
  $("#wifi-ap-save").on("click", saveWiFiAP);
}

function setFanMode(m) {
  fanMode = m;
  $("#fan-auto, #fan-manual").removeClass("active");
  if (m === 0) {
    $("#fan-auto").addClass("active");
    $("#fan-auto-controls").show();
    $("#fan-manual-controls").hide();
  } else {
    $("#fan-manual").addClass("active");
    $("#fan-manual-controls").show();
    $("#fan-auto-controls").hide();
  }
}

function setSvMode(m) {
  svMode = m;
  $("#sv-auto, #sv-manual").removeClass("active");
  if (m === 0) {
    $("#sv-auto").addClass("active");
    $("#sv-auto-controls").show();
    $("#sv-manual-controls").hide();
    loadSchedule();
  } else {
    $("#sv-manual").addClass("active");
    $("#sv-manual-controls").show();
    $("#sv-auto-controls").hide();
  }
}

function applyFan() {
  var data = { modo: fanMode };
  if (fanMode === 0) {
    data.temp_deseada = parseFloat($("#fan-td").val());
    data.temp_maxima = parseFloat($("#fan-tm").val());
  } else {
    data.manual_pct = parseInt($("#fan-mp").val());
  }
  $.ajax({
    url: '/api/fan', method: 'POST', contentType: 'application/json',
    data: JSON.stringify(data),
    success: function(){ syncFanForm(); }
  });
}

function applyServo() {
  if (svMode === 0) {
    $("#sv-schedule tbody tr").each(function(){
      var idx = $(this).data("index");
      var h = parseInt($(this).find(".s-hour").val());
      var m = parseInt($(this).find(".s-min").val());
      var p = parseInt($(this).find(".s-pct").val());
      var a = $(this).find(".s-act").is(":checked") ? 1 : 0;
      $.ajax({
        url: '/api/servo/schedule', method: 'POST', contentType: 'application/json',
        data: JSON.stringify({index: idx, hora: h, minuto: m, porcentaje: p, activo: a})
      });
    });
    $.ajax({
      url: '/api/servo', method: 'POST', contentType: 'application/json',
      data: JSON.stringify({modo: 0}),
      success: function(){ syncServoForm(); }
    });
  } else {
    var data = { modo: 1, manual_pct: parseInt($("#sv-mp").val()) };
    $.ajax({
      url: '/api/servo', method: 'POST', contentType: 'application/json',
      data: JSON.stringify(data),
      success: function(){ syncServoForm(); }
    });
  }
}

function applyRGB() {
  $.ajax({
    url: '/api/rgb', method: 'POST', contentType: 'application/json',
    data: JSON.stringify({
      red: parseInt($("#rgb-r").val()),
      green: parseInt($("#rgb-g").val()),
      blue: parseInt($("#rgb-b").val()),
      brillo: parseInt($("#rgb-br").val())
    }),
    success: function(){ syncRGBForm(); }
  });
}

function updateRGBPreview() {
  var r = parseInt($("#rgb-r").val());
  var g = parseInt($("#rgb-g").val());
  var b = parseInt($("#rgb-b").val());
  $("#rgb-preview").css("background","rgb("+r+","+g+","+b+")");
}

function saveWiFiSTA() {
  $.ajax({
    url: '/api/wifi/sta', method: 'POST', contentType: 'application/json',
    data: JSON.stringify({
      ssid: $("#wifi-sta-ssid").val(),
      password: $("#wifi-sta-pass").val()
    })
  });
}

function saveWiFiAP() {
  $.ajax({
    url: '/api/wifi/ap', method: 'POST', contentType: 'application/json',
    data: JSON.stringify({
      ssid: $("#wifi-ap-ssid").val(),
      password: $("#wifi-ap-pass").val()
    })
  });
}

// ============ SINCRONIZACION ============

function syncFormFromServer(d) {
  fanMode = d.fan_modo;
  if (fanMode === 0) {
    setFanMode(0);
    $("#fan-td").val(d.fan_temp_deseada); $("#fan-td-val").text(d.fan_temp_deseada);
    $("#fan-tm").val(d.fan_temp_maxima); $("#fan-tm-val").text(d.fan_temp_maxima);
  } else {
    setFanMode(1);
    $("#fan-mp").val(d.fan_manual_pct); $("#fan-mp-val").text(d.fan_manual_pct);
  }
  svMode = d.servo_modo;
  if (svMode === 0) { setSvMode(0); }
  else {
    setSvMode(1);
    $("#sv-mp").val(d.servo_manual_pct); $("#sv-mp-val").text(d.servo_manual_pct);
  }
  var rv = Math.round(d.int_red * 255);
  var gv = Math.round(d.int_green * 255);
  var bv = Math.round(d.int_blue * 255);
  var br = Math.round(d.brillo * 100);
  $("#rgb-r").val(rv); $("#rgb-r-val").text(rv);
  $("#rgb-g").val(gv); $("#rgb-g-val").text(gv);
  $("#rgb-b").val(bv); $("#rgb-b-val").text(bv);
  $("#rgb-br").val(br); $("#rgb-br-val").text(br);
  updateRGBPreview();
}

function syncFanForm() {
  $.getJSON('/api/status', function(d) {
    fanMode = d.fan_modo;
    if (fanMode === 0) {
      setFanMode(0);
      $("#fan-td").val(d.fan_temp_deseada); $("#fan-td-val").text(d.fan_temp_deseada);
      $("#fan-tm").val(d.fan_temp_maxima); $("#fan-tm-val").text(d.fan_temp_maxima);
    } else {
      setFanMode(1);
      $("#fan-mp").val(d.fan_manual_pct); $("#fan-mp-val").text(d.fan_manual_pct);
    }
  });
}

function syncServoForm() {
  $.getJSON('/api/status', function(d) {
    svMode = d.servo_modo;
    if (svMode === 0) { setSvMode(0); }
    else {
      setSvMode(1);
      $("#sv-mp").val(d.servo_manual_pct); $("#sv-mp-val").text(d.servo_manual_pct);
    }
  });
}

function syncRGBForm() {
  $.getJSON('/api/status', function(d) {
    var rv = Math.round(d.int_red * 255);
    var gv = Math.round(d.int_green * 255);
    var bv = Math.round(d.int_blue * 255);
    var br = Math.round(d.brillo * 100);
    $("#rgb-r").val(rv); $("#rgb-r-val").text(rv);
    $("#rgb-g").val(gv); $("#rgb-g-val").text(gv);
    $("#rgb-b").val(bv); $("#rgb-b-val").text(bv);
    $("#rgb-br").val(br); $("#rgb-br-val").text(br);
    updateRGBPreview();
  });
}

// ============ POLLING — SOLO BARRA DE ESTADO ============

function startStatusPoll() {
  getStatus();
  statusInterval = setInterval(getStatus, 3000);
}

function getStatus() {
  $.getJSON('/api/status', function(d) {
    var sym = d.temp_sym || "C";
    $("#s-temp").text(d.temp.toFixed(1) + " " + sym);
    $("#s-fan").text(d.fan_duty + "%");
    $("#s-servo").text(d.servo_posicion + "%");
    if (d.fan_alarma === 1) {
      $("#s-alarm").text("ALARMA").css("color","#ff5555");
    } else {
      $("#s-alarm").text("OK").css("color","#00e6c0");
    }
  });
}

// ============ TABLA DE HORARIOS ============

function loadSchedule() {
  $.getJSON('/api/servo/schedule', function(data) {
    var tbody = $("#sv-schedule tbody");
    tbody.empty();
    $.each(data, function(i, s){
      var tr = $("<tr>").data("index", s.index);
      tr.append($("<td>").text(s.index + 1));
      tr.append($("<td>").append($("<input class='s-hour' type='number' min='0' max='23' value='"+s.hora+"'>")));
      tr.append($("<td>").append($("<input class='s-min' type='number' min='0' max='59' value='"+s.minuto+"'>")));
      tr.append($("<td>").append($("<input class='s-pct' type='number' min='0' max='100' value='"+s.porcentaje+"'>")));
      tr.append($("<td>").append($("<input class='s-act' type='checkbox'>").prop("checked", s.activo===1)));
      tbody.append(tr);
    });
  });
}

// ============ OTA ============

function getFileInfo() {
  var x = document.getElementById("selected_file");
  var file = x.files[0];
  document.getElementById("file_info").innerHTML = "Archivo: "+file.name+" — "+file.size+" bytes";
}

function updateFirmware() {
  var formData = new FormData();
  var fs = document.getElementById("selected_file");
  if (fs.files && fs.files.length == 1) {
    var file = fs.files[0];
    formData.set("file", file, file.name);
    document.getElementById("ota_update_status").innerHTML = "Subiendo " + file.name + "...";
    var xhr = new XMLHttpRequest();
    xhr.upload.addEventListener("progress", updateProgress);
    xhr.open('POST', "/OTAupdate");
    xhr.responseType = "blob";
    xhr.send(formData);
  } else { alert('Seleccione un archivo primero'); }
}

function updateProgress(oEvent) {
  if (oEvent.lengthComputable) getUpdateStatus();
}

function getUpdateStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open('POST', "/OTAstatus", false);
  xhr.send('ota_update_status');
  if (xhr.readyState == 4 && xhr.status == 200) {
    var r = JSON.parse(xhr.responseText);
    document.getElementById("latest_firmware").innerHTML = r.compile_date + " - " + r.compile_time;
    if (r.ota_update_status == 1) {
      seconds = 10;
      otaRebootTimer();
    } else if (r.ota_update_status == -1) {
      document.getElementById("ota_update_status").innerHTML = "Error de subida!";
    }
  }
}

function otaRebootTimer() {
  document.getElementById("ota_update_status").innerHTML = "Actualizado. Reiniciando en: " + seconds;
  if (--seconds == 0) { clearTimeout(otaTimerVar); window.location.reload(); }
  else { otaTimerVar = setTimeout(otaRebootTimer, 1000); }
}

/* 
 * eagle510x.js
 * eagle32 board setup program (java script for setup pages)
 */

function formsubmit( fm )
{
    if( document.readyState ) {
        if (document.readyState == 'complete' ) {
            fm.submit();
        }
    }
    else {
        fm.submit();
    }
}

function on_system_click() {
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        dvrform.action="system.html";
        formsubmit( dvrform );
    } 
}

function on_camera_click() {
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        dvrform.action="camera.html";
        formsubmit( dvrform );
    } 
}
function on_camera_n_click( cameraid )
{
    document.getElementById("id_nextcameraid").value=cameraid ;
    on_camera_click();
}

function on_sensor_click() {
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        dvrform.action="sensor.html";
        formsubmit( dvrform );
    } 
}

function on_network_click() {
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        dvrform.action="network.html";
        formsubmit( dvrform );
    } 
}

function on_status_click() {
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        dvrform.action="status.html";
        formsubmit( dvrform );
    } 
}

function on_tools_click() {
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        dvrform.action="tools.html";
        formsubmit( dvrform );
    } 
}

function on_sync_time() {
    var t = new Date();
    document.getElementById("id_synctime").value = t.getTime() ;
    var dvrform = document.getElementById("dvrsetup");
    if( dvrform != null ) {
        formsubmit( dvrform );
    } 
}

function setfieldvalue( fieldid, fieldvalue )
{
    try {
        var elems = document.getElementsByName( fieldid ) ;
        if( elems != null && elems.length > 0 ) {
            for ( i=0; i<elems.length ; i++ ) 
            {
                if( elems[i].type == "checkbox" ) {
                    if( fieldvalue == "0" || fieldvalue == "off" ) {
                        elems[i].checked = null ;
                    }
                    else {
                        elems[i].checked = "checked" ;
                    }
                }
                else if( elems[i].type == "radio" ) {
                    if( elems[i].value == fieldvalue ) {
                        elems[i].checked = "checked" ;
                    }
                    else {
                        elems[i].checked = null ;
                    }
                }
                else {
                    elems[i].value = fieldvalue ;
                }
            }
        }
    }
    catch( err )
    {
        alert( "JavaScript Error: setfieldvalue() "+err.description+"\n\nClick OK to continue.\n\n");
    }
}

/* initialize input fields (JSON mode) */
function JSONinitfield( formdata )
{
    for( f in formdata ) {
        setfieldvalue( f, formdata[f] );
    }
}

function validate_config(form)
{
  var len = form.cfgupload_file.value.length;
  var ext = new Array('.','c','f','g');
  if (form.cfgupload_file.value == '') {
    alert("Please select a config file to upload!");
    return false;
  }
  var bin = form.cfgupload_file.value.toLowerCase();
  for (var i=0; i<4; i++) {
    if (ext[i] != bin.charAt(len-4+i)) {
      alert("Incorrect configuration file!");
      return false;
    }
  }

  return true;
}

function validate_firmware(form)
{
  var len = form.firmware_file.value.length;
  var ext = new Array('.','f','w');
  if (form.firmware_file.value == '') {
    alert("Please select a firmware file to upload!");
    return false;
  }
  var bin = form.firmware_file.value.toLowerCase();
  for (var i=0; i<3; i++) {
    if (ext[i] != bin.charAt(len-3+i)) {
      alert("Incorrect firmware file!");
      return false;
    }
  }

  return true;
}

function validate_mcu(form)
{
  var len = form.mcu_firmware_file.value.length;
  var ext = new Array('.','h','e','x');
  if (form.mcu_firmware_file.value == '') {
    alert("Please select a MCU firmware file to upload!");
    return false;
  }
  var bin = form.mcu_firmware_file.value.toLowerCase();
  for (var i=0; i<4; i++) {
    if (ext[i] != bin.charAt(len-4+i)) {
      alert("Incorrect MCU firmware file!");
      return false;
    }
  }

  return confirm("Plug in \"POWER FORCE ON\" cable now, and click OK");
}

/* 
     * eagle.js
     * eagle32 board setup program (java script for setup pages)
*/

var formdatardy = 0 ;

function formsubmit( fm )
{
    if( formdatardy==1 ) {
        if( document.readyState ) {
            if (document.readyState == 'complete' ) {
                fm.submit();
            }
        }
        else {
            fm.submit();
        }
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

function setfieldvalue( fieldid, fieldvalue )
{
    try {
        var elems = document.getElementsByName( fieldid ) ;
        if( elems != null ) 
        for( var i=0; i<elems.length; i++) {
            if( elems[i].type == "checkbox" ) {
            	elems[i].checked = ( fieldvalue == elems[i].value ) || ( fieldvalue=="on" ) || (fieldvalue==1) ;
            }
            else if( elems[i].type == "radio" ) {
            	elems[i].checked = ( fieldvalue == elems[i].value ) ;
            }
        		else {
        			elems[i].value = fieldvalue ;
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
    if( formdata ) {
        for( f in formdata ) {
            setfieldvalue( f, formdata[f] );
        }
    }
    formdatardy = 1 ;
}

/* Ajax */
function ajaxload(mode, url, loaded, fail)
{
    if (window.XMLHttpRequest)
    {   // code for IE7+, Firefox, Chrome, Opera, Safari
        xmlhttp=new XMLHttpRequest();
    }
    else
    {   // code for IE6, IE5
        xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
    }
    xmlhttp.onreadystatechange=function()
    {
        if (xmlhttp.readyState==4 ) {       // 4 = "loaded"
            if ( xmlhttp.status==200 ) {    // 200 = "OK"
                if( loaded ) {
                    loaded( xmlhttp.responseText );
                }
            }
            else {
                if( fail ) {
                    fail(xmlhttp.statusText);
                }
                else {
                    alert("Problem retrieving data:" + xmlhttp.statusText);
                }
            }
        }
    }
    xmlhttp.open(mode,url,true);
    xmlhttp.send();
}


<?

function returnIfExists($json, $setting) {
    if ($json == null) {
        return "";
    }
    if (array_key_exists($setting, $json)) {
        return $json[$setting];
    }
    return "";
}

function convertAndGetSettings() {
    global $settings;
        
    $cfgFile = $settings['configDirectory'] . "/plugin.fpp-osc.json";
    if (file_exists($cfgFile)) {
        $j = file_get_contents($cfgFile);
        $json = json_decode($j, true);
        return $json;
    }
    $j = "{\"port\": 9000, \"events\": [] }";
    return json_decode($j, true);
}

$pluginJson = convertAndGetSettings();
?>


<div id="global" class="settings">
<fieldset>
<legend>Open Sound Control Config</legend>

<script>




function PrintCommandArgsForOSC(tblCommand, configAdjustable, args) {
    var count = 1;
    var initFuncs = [];
    var haveTime = 0;
    var haveDate = 0;
    var children = [];

//    $.each( args,
    var valFunc = function( key, val ) {
        if (val['type'] == 'args') {
            return;
        }

        if ((val.hasOwnProperty('statusOnly')) &&
            (val.statusOnly == true)) {
            return;
        }
        var ID = tblCommand + "_arg_" + count;
        var line = "<tr id='" + ID + "_row' class='arg_row_" + val['name'] + "'><td>";

        if (children.includes(val['name']))
            line += "&nbsp;&nbsp;&nbsp;&nbsp;&bull;&nbsp;";

        var typeName = val['type'];
        if (typeName == "datalist") {
            typeName = "string";
        }
        line += val["description"] + " (" + typeName + "):</td><td>";

        var dv = "";
        if (typeof val['default'] != "undefined") {
            dv = val['default'];
        }
        var contentListPostfix = "";
        line += "<input class='arg_" + val['name'] + "' id='" + ID  + "' type='text' size='40' maxlength='200' data-osc-type='" + typeName + "' ";
        if (val['type'] == "datalist" ||  (typeof val['contentListUrl'] != "undefined")) {
            line += " list='" + ID + "_list' value='" + dv + "'";
        } else if (val['type'] == "bool") {
            if (dv == "true" || dv == "1") {
                line += " value='true'";
            } else {
                line += " value='false'";
            }
        } else if (val['type'] == "time") {
            line += " value='00:00:00'";
        } else if (val['type'] == "date") {
            line += " value='2020-12-25'";
        } else if ((val['type'] == "int") || (val['type'] == "float")) {
            if (dv != "") {
                line += " value='" + dv + "'";
            } else if (typeof val['min'] != "undefined") {
                line += " value='" + val['min'] + "'";
            }
        } else if (dv != "") {
            line += " value='" + dv + "'";
        }
        line += ">";
        if ((val['type'] == "int") || (val['type'] == "float")) {
            if (typeof val['unit'] === 'string') {
                line += ' ' + val['unit'];
            }
        }
        line +="</input>";
        if (val['type'] == "datalist" || (typeof val['contentListUrl'] != "undefined")) {
            line += "<datalist id='" + ID + "_list'>";
            $.each(val['contents'], function( key, v ) {
                   line += '<option value="' + v + '"';
                   line += ">" + v + "</option>";
                   });
            line += "</datalist>";
            contentListPostfix = "_list";
        }

        line += "</td></tr>";
        $('#' + tblCommand).append(line);
        if (typeof val['contentListUrl'] != "undefined") {
            var selId = "#" + tblCommand + "_arg_" + count + contentListPostfix;
            $.ajax({
                   dataType: "json",
                   url: val['contentListUrl'],
                   async: false,
                   success: function(data) {
                       if (Array.isArray(data)) {
                            data.sort();
                            $.each( data, function( key, v ) {
                              var line = '<option value="' + v + '"';
                              if (v == dv) {
                                line += " selected";
                              }
                              line += ">" + v + "</option>";
                              $(selId).append(line);
                            });
                       } else {
                            $.each( data, function( key, v ) {
                                   var line = '<option value="' + key + '"';
                                   if (key == dv) {
                                        line += " selected";
                                   }
                                   line += ">" + v + "</option>";
                                   $(selId).append(line);
                            });
                       }
                   }
                   });
        }
        count = count + 1;
    };
    $.each( args, valFunc);
}

function ConditionTypeChanged( id ) {
    var val = $('#conditionSelect_' + id).val();
    if (val === 'ALWAYS') {
        $("#conditionTypeSelect_" + id).hide();
        $("#conditionText_" + id).hide();
    } else {
        $("#conditionTypeSelect_" + id).show();
        $("#conditionText_" + id).show();
    }
}
function AddOSC() {
    var id = $("#oscEventTable > tbody").children().length + 1;
    
    var html = "<tr id='row_" + id + "' class='fppTableRow'";
    if (id % 2 == 0) {
        html += " style='background: #FFFFFF;'";
    }
    html += "><td style='vertical-align: top;'><input type='text' size='50' id='desc_" + id + "'></td>";
    html += "<td style='vertical-align: top;'><input type='text' size='50' id='path_" + id + "'></td>";
    html += "<td style='vertical-align: top;'>";
    html += "<select id='conditionSelect_" + id + "' onChange='ConditionTypeChanged(" + id + ")'>";
    html += "<option value='ALWAYS'>Always</option><option value='p1'>Param1</option><option value='p2'>Param2</option>";
    html += "<option value='p3'>Param3</option><option value='p4'>Param4</option><option value='p5'>Param5</option></select>";
    html += "<select id='conditionTypeSelect_" + id + "' style='display:none;'><option value='='>=</option>";
    html += "<option value='='>=</option><option value='!='>!=</option>";
    html += "<option value='&lt;'>&lt;</option><option value='&lt;='>&lt;=</option>";
    html += "<option value='&gt;'>&gt;</option><option value='&gt;='>&gt;=</option>";
    html += "<option value='contains'>Contains</option><option value='iscontainedin'>Is In</option>";
    html += "<input type='text' size='20' id='conditionText_" + id + "' style='display:none;'>";
    html += "</td><td style='vertical-align: top;'><table class='fppTable' border=0 id='tableOSCCommand_" + id +"'>";
    html += "<tr><td>Command:</td><td><select id='osccommand" + id + "' onChange='CommandSelectChanged(\"osccommand" + id + "\", \"tableOSCCommand_" + id + "\" , false, PrintCommandArgsForOSC);'><option value=''></option></select></td></tr>";
    html += "</table></td></tr>";
    
    $("#oscEventTable > tbody").append(html);
    LoadCommandList($('#osccommand' + id));

    return id;
}

function RemoveOSC() {
    
}

var oscConfig = <? echo json_encode($pluginJson, JSON_PRETTY_PRINT); ?>;
function SaveOSCConfig(config) {
    var data = JSON.stringify(config);
    $.ajax({
        type: "POST",
        url: 'fppjson.php?command=setPluginJSON&plugin=fpp-osc',
        dataType: 'json',
        async: false,
        data: data,
        processData: false,
        contentType: 'application/json',
        success: function (data) {
        }
    });
}

function SaveEvent(id) {
    var desc = $('#desc_' + id).val();
    var path = $('#path_' + id).val();
    var cond = $('#conditionSelect_' + id).val();
    var condType = $('#conditionTypeSelect_' + id).val();
    var condText = $('#conditionText_' + id).val();
    
    var json = {
        "description": desc,
        "path": path,
        "conditions": [
            {
                "condition": cond,
                "conditionCompare": condType,
                "conditionText": condText
            }
        ]
    };
    CommandToJSON('osccommand' + id, 'tableOSCCommand_' + id, json, "osc-type");
    return json;
}


function SaveOSC() {
    var port = parseInt($("#portSpin").val());
    oscConfig = { "port": port, "events": []};
    var count = $("#oscEventTable > tbody").children().length;
    for (var x = 1; x <= count; x++) {
        oscConfig["events"][x-1] = SaveEvent(x);
    }
    
    SaveOSCConfig(oscConfig);
}
function RefreshLastMessages() {
    $.get('api/plugin-apis/OSC/Last', function (data) {
          $("#lastMessages").text(data);
        }
    );
}
</script>
<div>
<span style="float:right">
<table border=0>
<tr><td style='vertical-align: top;'>Last Messages:<br><input type="button" value="Refresh" class="buttons" onclick="RefreshLastMessages();"></td><td style='vertical-align: top;'><pre id="lastMessages" style='min-width:200px; margin:1px;'></pre></td></tr>
</table>
</span>
<span>
<table border=0>
<tr><td>Listen&nbsp;Port:</td><td width="200px"><input type='number' id='portSpin' min='1' max='65535' size='10' value='<? echo $pluginJson["port"] ?>'></input></td></tr>
<tr><td><input type="button" value="Save" class="buttons" onclick="SaveOSC();"></td>
    <td><input type="button" value="Add" class="buttons" onclick="AddOSC();"><input id="delButton" type="button" value="Delete" class="buttons" onclick="RemoveOSC();"></td></tr>

</table>
</span>
</div>

<table class="fppTable" id="oscEventTable">
<thead><tr class="fppTableHeader"><th>Description</th><th>Path</th><th>Conditions</th><th>Command</th></tr></thead>
<tbody>
</tbody>
</table>

<script>
$.each(oscConfig["events"], function( key, val ) {
    var id = AddOSC();
    $('#desc_' + id).val(val["description"]);
    $('#path_' + id).val(val["path"]);
    $('#conditionSelect_' + id).val(val["conditions"][0]["condition"]);
    ConditionTypeChanged(id);
    $('#conditionTypeSelect_' + id).val(val["conditions"][0]["conditionCompare"]);
    $('#conditionText_' + id).val(val["conditions"][0]["conditionText"]);
    PopulateExistingCommand(val, 'osccommand' + id, 'tableOSCCommand_' + id, false, PrintCommandArgsForOSC);
});
RefreshLastMessages();
</script>

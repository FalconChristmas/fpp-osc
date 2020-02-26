
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

function ConditionTypeChanged(item) {
    var val = $(item).find('.conditionSelect').val();
    if (val === 'ALWAYS') {
        $(item).find(".conditionTypeSelect").hide();
        $(item).find(".conditionText").hide();
    } else {
        $(item).find(".conditionTypeSelect").show();
        $(item).find(".conditionText").show();
    }
}

function AddOption(value, text, current) {
    var o = "<option value='" + value + "'";

    if (value == current)
        o += " selected";

    o += ">" + text + "</option>";

    return o;
}

function RemoveCondition(item) {
    if ($(item).parent().find('tr').length == 1)
        return;

    $(item).remove();
}

function AddCondition(row, condition, compare, text) {
    var rows = $(row).find('.conditions > tr').length;
    var c = "<tr>";

    if (rows == 0)
        c += "<td><a href='#' class='addButton' onClick='AddCondition($(this).parent().parent().parent().parent(), \"ALWAYS\", \"\", \"\");'></a></td>";
    else
        c += "<td><a href='#' class='deleteButton' onClick='RemoveCondition($(this).parent().parent());'></a></td>";

    c += "<td><select class='conditionSelect' onChange='ConditionTypeChanged($(this).parent());'>";
    c += AddOption('ALWAYS', 'Always', condition);
    c += AddOption('p1', 'Param1', condition);
    c += AddOption('p2', 'Param2', condition);
    c += AddOption('p3', 'Param3', condition);
    c += AddOption('p4', 'Param4', condition);
    c += AddOption('p5', 'Param5', condition);
    c += "</select>";

    c += "<select class='conditionTypeSelect' style='display:none;'>";
    c += AddOption('=', '=', compare);
    c += AddOption('!=', '!=', compare);
    c += AddOption('&lt;', '&lt;', compare);
    c += AddOption('&lt;=', '&lt;=', compare);
    c += AddOption('&gt;', '&gt;', compare);
    c += AddOption('&gt;=', '&gt;=', compare);
    c += AddOption('contains', 'Contains', compare);
    c += AddOption('iscontainedin', 'Is In', compare);
    c += "</select>";

    c += "<input type='text' size='18' maxlength='30' class='conditionText' style='display:none;' value='" + text + "'>";

    c += "</td></tr>";

    $(row).find('.conditions').append(c);

    ConditionTypeChanged($(row).find('.conditions > tr').last());
}

var uniqueId = 1;
function AddOSC() {
    var id = $("#oscEventTableBody > tr").length + 1;
    
    var html = "<tr class='fppTableRow";
    if (id % 2 != 0) {
        html += " oddRow'";
    }
    html += "'><td class='colNumber rowNumber'>" + id + ".<td><input type='text' size='25' maxlength='50' class='desc'><span style='display: none;' class='uniqueId'>" + uniqueId + "</span></td>";
    html += "<td><input type='text' size='30' maxlength='50' class='path'></td>";
    html += "<td><table><tbody class='conditions'></tbody></table>";
    html += "</td><td><table class='fppTable' border=0 id='tableOSCCommand_" + uniqueId +"'>";
    html += "<tr><td>Command:</td><td><select class='osccommand' id='osccommand" + uniqueId + "' onChange='CommandSelectChanged(\"osccommand" + uniqueId + "\", \"tableOSCCommand_" + uniqueId + "\" , false, PrintCommandArgsForOSC);'><option value=''></option></select></td></tr>";
    html += "</table></td></tr>";
    
    $("#oscEventTableBody").append(html);
    LoadCommandList($('#osccommand' + uniqueId));

    newRow = $('#oscEventTableBody > tr').last();
    $('#oscEventTableBody > tr').removeClass('selectedEntry');
    DisableButtonClass('deleteEventButton');

    uniqueId++;

    return newRow;
}

function RemoveOSC() {
    if ($('#oscEventTableBody').find('.selectedEntry').length) {
        $('#oscEventTableBody').find('.selectedEntry').remove();
        RenumberEvents();
    }

    DisableButtonClass('deleteEventButton');
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

function SaveEvent(row) {
    var desc = $(row).find('.desc').val();
    var path = $(row).find('.path').val();
    var conditions = [];

    $(row).find('.conditions > tr').each(function() {
        var cond     = $(this).find('.conditionSelect').val();
        var condType = $(this).find('.conditionTypeSelect').val();
        var condText = $(this).find('.conditionText').val();

        var condition = {};
        condition.condition = cond;
        condition.conditionCompare = condType;
        condition.conditionText = condText;
        conditions.push(condition);
    });

    var id = $(row).find('.uniqueId').html();
    
    var json = {
        "description": desc,
        "path": path,
        "conditions": conditions
    };
    CommandToJSON('osccommand' + id, 'tableOSCCommand_' + id, json, "osc-type");
    return json;
}


function SaveOSC() {
    var port = parseInt($("#portSpin").val());
    oscConfig = { "port": port, "events": []};
    var i = 0;
    $("#oscEventTableBody > tr").each(function() {
        oscConfig["events"][i++] = SaveEvent(this);
    });
    
    SaveOSCConfig(oscConfig);
}
function RefreshLastMessages() {
    $.get('api/plugin-apis/OSC/Last', function (data) {
          $("#lastMessages").text(data);
        }
    );
}

function RenumberEvents() {
    var id = 1;
    $('#oscEventTableBody > tr').each(function() {
        $(this).find('.rowNumber').html('' + id++ + '.');
        $(this).removeClass('oddRow');

        if (id % 2 != 0) {
            $(this).addClass('oddRow');
        }
    });
}

$(document).ready(function() {

    $('#oscEventTableBody').sortable({
        update: function(event, ui) {
            RenumberEvents();
        },
        item: '> tr',
        scroll: true
    }).disableSelection();

    $('#oscEventTableBody').on('mousedown', 'tr', function(event,ui){
        $('#oscEventTableBody tr').removeClass('selectedEntry');
        $(this).addClass('selectedEntry');
        EnableButtonClass('deleteEventButton');
    });

});

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
<tr><td colspan='2'>
        <input type="button" value="Save" class="buttons genericButton" onclick="SaveOSC();">
        <input type="button" value="Add" class="buttons genericButton" onclick="AddCondition(AddOSC(), 'ALWAYS', '', '');">
        <input id="delButton" type="button" value="Delete" class="deleteEventButton disableButtons genericButton" onclick="RemoveOSC();">
    </td>
</tr>
</table>
</span>
</div>

<div class='genericTableWrapper'>
<div class='genericTableContents'>
<table class="fppTable" id="oscEventTable">
<thead><tr class="fppTableHeader"><th>#</th><th>Description</th><th>Path</th><th>Conditions</th><th>Command</th></tr></thead>
<tbody id='oscEventTableBody'>
</tbody>
</table>
</div>
</div>

<script>
$.each(oscConfig["events"], function( key, val ) {
    var row = AddOSC();
    $(row).find('.desc').val(val["description"]);
    $(row).find('.path').val(val["path"]);

    for (var i = 0; i < val['conditions'].length; i++) {
        AddCondition(row,
            val['conditions'][i]['condition'],
            val['conditions'][i]['conditionCompare'],
            val['conditions'][i]['conditionText']);
    }
    var id = parseInt($(row).find('.uniqueId').html());
    PopulateExistingCommand(val, 'osccommand' + id, 'tableOSCCommand_' + id, false, PrintCommandArgsForOSC);
});
RefreshLastMessages();
</script>
</fieldset>
</div>

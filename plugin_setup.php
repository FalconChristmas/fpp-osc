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
<legend>Open Sound Control Config</legend>

<script>
allowMultisyncCommands = true;


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

    
    var realVal = $('<textarea />').html(value).text();
    if (value == current || realVal == current)
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
        c += "<td><button class='circularButton circularButton-vsm circularButton-sm circularButton-visible circularAddButton' onClick='AddCondition($(this).parent().parent().parent().parent(), \"ALWAYS\", \"\", \"\");'>Add</button></td>";
    else
        c += "<td><button class='circularButton circularButton-vsm circularButton-sm circularButton-visible circularDeleteButton' onClick='RemoveCondition($(this).parent().parent());'>Delete</button></td>";

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

    c += "<input type='text' size='12' maxlength='30' class='conditionText' style='display:none;' value='" + text + "'>";

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
    html += "'><td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>";
    html += "<td><input type='text' size='25' maxlength='50' class='desc'><span style='display: none;' class='uniqueId'>" + uniqueId + "</span></td>";
    html += "<td><input type='text' size='30' maxlength='50' class='path'></td>";
    html += "<td><table><tbody class='conditions'></tbody></table>";
    html += "</td><td><table class='fppTable' border=0 id='tableOSCCommand_" + uniqueId +"'>";
    html += "<tr><td>Command:</td><td><select class='osccommand' id='osccommand" + uniqueId + "' onChange='CommandSelectChanged(\"osccommand" + uniqueId + "\", \"tableOSCCommand_" + uniqueId + "\" , false, PrintArgsInputsForEditable);'><option value=''></option></select></td></tr>";
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
    }

    DisableButtonClass('deleteEventButton');
}

var oscConfig = <? echo json_encode($pluginJson, JSON_PRETTY_PRINT); ?>;
function SaveOSCConfig(config) {
    var data = JSON.stringify(config);
    $.ajax({
        type: "POST",
        url: 'api/configfile/plugin.fpp-osc.json',
        dataType: 'json',
        async: false,
        data: data,
        processData: false,
        contentType: 'application/json',
        success: function (data) {
           SetRestartFlag(2);
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
    CommandToJSON('osccommand' + id, 'tableOSCCommand_' + id, json, true);
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

$(document).ready(function() {
    $('#oscEventTableBody').sortable({
        update: function(event, ui) {
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
<div class="row">
    <div class="col-auto mr-auto">
        <div class="row">
            <div class="col-auto">
                Listen&nbsp;Port: &nbsp;<input type='number' id='portSpin' min='1' max='65535' size='10' value='<? echo $pluginJson["port"] ?>'></input>
            </div>
        </div>
        <div class="row">
            <div class="col-auto">
                <input type="button" value="Save" class="buttons genericButton" onclick="SaveOSC();">
                <input type="button" value="Add" class="buttons genericButton" onclick="AddCondition(AddOSC(), 'ALWAYS', '', '');">
                <input id="delButton" type="button" value="Delete" class="deleteEventButton disableButtons genericButton" onclick="RemoveOSC();">
            </div>
        </div>
        <div class="row">
            <div class="col-auto">
                <div class='fppTableWrapper'>
                    <div class='fppTableContents'>
                        <table class="fppSelectableRowTable" id="oscEventTable"  width='100%'>
                        <thead><tr class="fppTableHeader"><th>#</th><th>Description</th><th>Path</th><th>Conditions</th><th>Command</th></tr></thead>
                        <tbody id='oscEventTableBody' class="ui-sortable">
                        </tbody>
                        </table>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <div class="col-auto">
        <div>
            <div class="row">
                <div class="col">
                    Last Messages:&nbsp;<input type="button" value="Refresh" class="buttons" onclick="RefreshLastMessages();">
                </div>
            </div>
            <div class="row">
                <div class="col">
                    <pre id="lastMessages" style='min-width:150px; margin:1px;min-height:300px;'></pre>
                </div>
            </div>
        </div>
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
    PopulateExistingCommand(val, 'osccommand' + id, 'tableOSCCommand_' + id, false, PrintArgsInputsForEditable);
});
RefreshLastMessages();
</script>
</div>

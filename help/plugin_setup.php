The Open Sound Control (OSC) Plugin can be use to respond to OSC events by invoking FPP Commands.
<p>
The Listen Port is the port that FPP will listen on.  This needs to match the port the OSC controller is sending events to.
<p>
For each Event added, the following fields need to be configured:
<p>
<ol>
<li>Description - this is a short description of what the event does.  This is ignored by FPP, but can be used to help you organized the events.</li>
<li>Path - this is the path the OSC controller will be sending the desired event.</li>
<li>Conditions - these are conditions to filter in/out events based on parameters in the message sent from the OSC controller.  For example, you could apply a condition to only respond to button down states insead of up and down.</li>
<li>Command - the FPP Command to execute.   If the paramter starts with a single equal sign, it will be evaluated as a simple mathamatical formula.  For example, if parameter 1 is a "float" from a slider control ranging from 0.0-1.0 and you need the value to be 0-100 (example: Volume percent), then the command argument can be entred as "=p1*100".
<p>
If the parameter does not start with a single =, it is treated as a string, but parameters can be sustituted in by using %%p1%% in the string.  For example: "Matrix-%%p1%%".
<p>
    The plugin uses the TinyExpr library from https://github.com/codeplea/tinyexpr for implementing the expression processing.   We have added three useful functions:<br>
    <ul>
        <li>rgb(r, g, b) - will take the r/g/b values (0-255) and create an integer to represent the color </li>
        <li>hsv(h, s, v) - will take the hue/saturation/value values (0-1) and create an integer to represent the color </li>
        <li>if(cond, tExp, fExp) - will evaluate the condition and if not 0, will return tExp, otherwise fExp</li>
    </ul>
</li>
</ol>
<p>
The "Last Messages" section in the upper right displays the last 10 messages that FPPD has received.  Clicking Refresh will refresh the list.  These can be used to help identify which parameters are being used to help define conditions.

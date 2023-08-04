# VGA Space Info
Displays status-tiles for things that should be done before leaving the space, like closing the windows.

## REST API

### Setting a Layout
Layouts are set via JSON sent to `/layout` as HTTP POST requests. For example like this:
```json
{
	"solder":{"x":0,"y":0,"desc":["Alle","Lötstationen","abgekühlt?"], "icon":"soldering_iron"},
	"power":{"x":1,"y":0,"desc":["Strom","ist","aus?"], "icon":"building_power"},
	"dishwasher":{"x":2,"y":0,"desc":["Spülmaschine","ist","ausgeräumt?"], "icon":"dishwasher_alert", "icon_ok":"dishwasher"},
	"shutters":{"x":0,"y":1,"desc":["Rolläden","alle","geschlossen?"], "icon":"shutter_open", "icon_ok":"shutter_closed"},
	"windows":{"x":1,"y":1,"desc":["Sind","alle Fenster","zu?"], "icon":"window_open", "icon_ok":"window"},
	"door":{"x":2,"y":1,"desc":["Hubwagen","Tür","geschlossen?"], "icon":"door_open", "icon_ok":"door_closed"}
}
```
A layout consists of a single object with each key being the name of your choice for a status-tile.
The value corresponding to that key is another object which has the `x` and `y` keys for the tiles location on the 3x2 grid,
a `desc` key for an array with three strings (character set is [CodePage437](https://en.wikipedia.org/wiki/Code_page_437) -> encoding ANSI), each one line of the description,
and optionally an `icon` key for a string corresponding to the name of an icon.

### Setting states
States are set via JSON sent to `/layout` as HTTP POST requests. For example like this:
```json
{"solder":"unknown","power":"on","dishwasher":"off",
"shutters":"unknown","windows":"off","door":"on"}
```
For each tile that shall change state, a key-value pair has to be sent, the key being the name assigned in the layout,
and the value being a string (either `on` for OK, `off` for bad, or anything else for unknown). 

### Other
* `brighness` sets the displays brightness. (Send a number from 0 to 100 via HTTP POST.)
* `power` turns the Display on/off via DDC. (Send either `on` or `off` via HTTP POST.)

## HomeAssistant Examples
### configuration.yaml
```yaml
rest_command:
  set_info_tile_states:
    url: "http://your.ip/states"
	payload: >
		{% macro entity_to_kv(entity_id) %}'{{ entity_id }}': '{{ states(entity_id) }}'{% endmacro %}
		{
			{{ entity_to_kv('binary_sensor.window_1') }},
			{{ entity_to_kv('binary_sensor.door_3') }}
		}
```

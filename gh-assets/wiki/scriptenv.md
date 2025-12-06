## Datatypes

Additional types with methods and properties

### `Color`

* An RGB color
#### `Color.new(R: number, G: number, B: number): Color`
* Returns a Color with the provided R, G, and B values
* Values are expected to be in the range 0 to 1
#### `.R: number`
* The Red channel

#### `.G: number`
* The Green channel

#### `.B: number`
* The Blue channel

### `EventConnection`

* A connection to an Event
#### `.Connected: boolean`
* Whether the Connection is connected or not

#### `Disconnect(): nil`
* Disconnects the Connection, causing the callback to no longer be invoked. May only be called once

#### `.Signal: any`
* The Event which `:Connect` was called upon to return this Connection

### `EventSignal`

* An Event that can be connected to
#### `Connect((any) -> (any)): EventConnection`
* Connect a callback to the Event

#### `WaitUntil(): any`
* Yields the thread until the Event is fired

### `GameObject`

* A Game Object
* Game Objects have a "base" API (properties, methods, and events) that can be extended by giving them Components
* The base API will be documented on the Component APIs page
#### `GameObject.validcomponents: { string }`
* A list of all valid component names which can be passed into `.new`
#### `GameObject.new(Component: string): GameObject`
* Creates a new GameObject with the provided Component
### `Matrix`

* A 4x4 transformation matrix
#### `Matrix.fromTranslation(Position: vector): Matrix`
#### `Matrix.fromTranslation(X: number, Y: number, Z: number): Matrix`
* Creates a Matrix which has been translated to the given coordinates (specified as either a `vector` or the individual X, Y, and Z components)
#### `Matrix.new(): Matrix`
* Creates a new identity matrix
#### `Matrix.lookAt(Eye: vector, Target: vector): Matrix`
* Creates a Matrix at position `Eye` oriented such that the `.Forward` vector moves toward `Target`
#### `Matrix.identity: Matrix`
* An identity matrix, the same value as what gets returned with `.new()` with no arguments
#### `Matrix.fromEulerAnglesXYZ(X: number, Y: number, Z: number): Matrix`
* Creates a Matrix which has been rotated by the given Euler angles (in radians) with the rotation order X-Y-Z
#### `__mul(Matrix): Matrix`
* Two Matrices may be multiplied together with the `*` operator

#### `.Right: vector`
* The rightward vector of the Matrix

#### `.C1R1: number`
* The value at Column 1, Row 1

#### `.C4R1: number`
* The value at Column 4, Row 1

#### `.C3R1: number`
* The value at Column 3, Row 1

#### `.C2R1: number`
* The value at Column 2, Row 1

#### `.C1R2: number`
* The value at Column 1, Row 2

#### `.C3R2: number`
* The value at Column 3, Row 2

#### `.C4R2: number`
* The value at Column 4, Row 2

#### `.Up: vector`
* The upward vector of the Matrix

#### `.C4R3: number`
* The value at Column 4, Row 3

#### `.C3R3: number`
* The value at Column 3, Row 3

#### `.C2R3: number`
* The value at Column 2, Row 3

#### `.C1R3: number`
* The value at Column 1, Row 3

#### `.C2R2: number`
* The value at Column 2, Row 2

#### `.Forward: vector`
* The forward vector of the Matrix

#### `.Position: vector`
* The position of the Matrix in world-space

#### `.C1R4: number`
* The value at Column 1, Row 4

#### `.C2R4: number`
* The value at Column 2, Row 4

#### `.C3R4: number`
* The value at Column 3, Row 4

#### `.C4R4: number`
* The value at Column 4, Row 4

## Libraries

Libraries specific to the Phoenix Engine Luau runtime (Luhx)

### `Enum`
* Contains enumerations
#### `Enum.Key: { [string]: number }`
* Keys, which may be passed to `input.keypressed`
#### `Enum.MouseButton: { Left: number, Middle: number, Right: number }`
* Mouse buttons, which may be passed to `input.mousedown`
### `conf`
* Internal Engine configuration state, loaded from file (`phoenix.conf`) upon startup
#### `conf.save(): boolean`
* Save the current state of the configuration to file (`phoenix.conf`), returning whether the file was successfully overwritten
#### `conf.set(Key: string, Value: any): `
* Change a specific value in the configuration
#### `conf.get(Key: string): any`
* Read a specific value from the configuration
### `fs`
* Interacting with the player's filesystem
#### `fs.makecwdaliasof(Alias: string): `
* Changes the meaning of non-qualified paths (e.g.: not beginning with `.` or `C:` etc) to act as aliases to the given path instead of referring to the CWD
#### `fs.write(Path: string, Contents: string, CreateDirectories: boolean? [ false ]): boolean, string?`
* Overwrites/creates the file at `Path` with the provided `Contents`
* Returns whether the operation was successful
* If `CreateDirectories` is `true`, will automatically create missing intermediate directories to the file
#### `fs.isfile(string): boolean`
* Returns whether the specified Path refers to a file
#### `fs.promptopenfolder(Title: string, DefaultDirectory: string?): string?`
* Prompts the user to select a folder
* Returns `nil` if no selection was made
#### `fs.rename(Path: string, NewName: string): boolean, string?`
* Renames the file at the given `Path` to have the `NewName`
#### `fs.remove(Path: string): number | false, string?`
* Removes everything at the given path
* On success, returns the number of items removed
* On failure, returns `false` and an error message
#### `fs.listdir(Path: string, Filter: ('a' | 'f' | 'd')? [ 'a' ]): { [string]: 'f' | 'd' }`
* Returns a table of all the entries in the specified directory
* The keys of the table is the path of the entry, while the values are the type of the entry
* `f` is file, `d` is directory, and `a` is all
#### `fs.read(Path: string): string?, string?`
* Reads the file at the given address, and returns the contents
* If the file could not be read, returns `nil` and an error message
#### `fs.copy(From: string, To: string): boolean, string?`
* Copies the file/directory at the given path to a new path
* Returns whether the operation was successful
* If unsuccessful, an error message is also returned
#### `fs.mkdir(Path: string): boolean, string?`
* Creates a directory at the given path
* Returns whether a directory was actually created
* Will return false if the directory already exists, but the error message will be `nil`
#### `fs.promptopen(DefaultLocation: string? [ './'  ], Filter: ( { string } | string)? [ '*.*' ], FilterName: string? [ 'All files' ], AllowMultipleFiles: boolean? [ false ]): { string }`
* Prompts the player to select a file to open
* Returns the list of files the player selected, or an empty list if the player cancelled the dialog
* If the operation fails, returns `nil` instead
#### `fs.promptsave(DefaultLocation: string? [ './' ], Filter: ( { string } | string)? [ '*.*' ], FilterName: string? [ 'All files' ]): string`
* Prompts the player to select a path to save a file to
* Returns the path the player selected, or `nil` if they cancelled the dialog
#### `fs.definealias(Alias: string, For: string): `
* Defines an alias for the filesystem, such that `@<Alias>/` refers to `<For>/` to the Engine
#### `fs.isdirectory(string): boolean`
* Returns whether the specified Path refers to a directory
#### `fs.cwd(): string`
* Returns the Current Working Directory of the Engine
### `imgui`
* UI with Dear ImGui
#### `imgui.setcursorposition(X: number, Y: number): `
* Sets the position of the Dear ImGui element cursor within the current window
#### `imgui.beginmainmenubar(): boolean`
* `ImGui::BeginMainMenuBar`
#### `imgui.endmenubar(): `
* `ImGui::EndMenuBar`
#### `imgui.dummy(Width: number? [ 0 ], Height: number? [ 0 ]): `
* `ImGui::Dummy`
#### `imgui.image(ImagePath: string, Size: vector?, FlipVertically: boolean? [ false ], TintColor: { number }? [ { 1, 1, 1, 1 } ]): `
* `ImGui::Image`
#### `imgui.openpopup(Name: string): `
* `ImGui::OpenPopup`
#### `imgui.settooltip(Tooltip: string): `
* `ImGui::SetTooltip`
#### `imgui.button(Text: string, Width: number?, Height: number?): boolean`
* `ImGui::Button`
#### `imgui.menuitem(Text: string, Enabled: boolean? [ true ]): boolean`
* `ImGui::MenuItem`
#### `imgui.inputstring(Name: string, Value: string): string`
* `ImGui::InputText`
#### `imgui.checkbox(Name: string, Value: boolean): boolean`
* `ImGui::Checkbox`
#### `imgui.text(Text: string): `
* `ImGui::Text`
#### `imgui.windowposition(): number, number`
* Returns the position of the current Dear ImGui window (`ImGui::GetWindowPos`)
* To *set* the position of the window, use `.setnextwindowposition`
#### `imgui.setnextwindowsize(Width: number, Height: number): `
* `ImGui::SetNextWindowSize`
#### `imgui.endmenu(): `
* `ImGui::EndMenu`
#### `imgui.setviewportdockspace(PositionX: number, PositionY: number, SizeX: number, SizeY: number): `
* Sets the region of the window in which Dear ImGui windows can be docked
#### `imgui.windowhovered(): boolean`
* `ImGui::IsWindowHovered`
#### `imgui.sameline(): `
* `ImGui::SameLine`
#### `imgui.begin(WindowTitle: string, WindowFlags: string? [ "" ]): boolean`
* `ImGui::Begin`
* `WindowFlags` may be a combination of any of the following sequences, optionally separated by ' ' or '|' characters: `nt` (no titlebar), `nr` (no resize), `nm` (no move), `nc` (no collapse), `ns` (no saved settings), `ni` (no interact), `h` (horizontal scrolling)
#### `imgui.inputnumber(Text: string, Value: number): number`
* `ImGui::InputDouble`
#### `imgui.treepop(): `
* `ImGui::TreePop`
#### `imgui.separator(): `
* `ImGui::Separator`
#### `imgui.treenode(Text: string): boolean`
* `ImGui::TreeNode`
#### `imgui.setnextwindowopen(Open: boolean? [ true ]): `
* `ImGui::SetNextWindowCollapsed(!Open)`
#### `imgui.endpopup(): `
* `ImGui::EndPopup`
#### `imgui.textsize(Text: string): number, number`
* `ImGui::CalcTextSize`
#### `imgui.textlink(Text: string): boolean`
* `ImGui::TextLink`
#### `imgui.endw(): `
* `ImGui::End`, `endw` and not `end` because `end` is a Luau keyword
#### `imgui.windowsize(): number, number`
* Returns the size of the current window
#### `imgui.combo(Text: string, Options: { string }, CurrentOption: number): number`
* `ImGui::Combo`. Returns the selected option index
#### `imgui.cursorposition(): number, number`
* Returns the current position of the Dear ImGui element cursor within the current window
* To *set* the cursor position, use `.setcursorposition`
#### `imgui.stylecolors(Theme: 'l' | 'd'): `
* `ImGui::StyleColorsDark`/`ImGui::StyleColorsLight`
#### `imgui.itemhovered(): boolean`
* `ImGui::IsItemHovered`
#### `imgui.setnextwindowposition(X: number, Y: number): `
* `ImGui::SetNextWindowPos
#### `imgui.setnextwindowfocus(): `
* `ImGui::SetNextWindowFocus`
#### `imgui.setviewportdockspacedefault(): `
* Reverts the region of the window in which Dear ImGui windows can be docked to the default settings
#### `imgui.setitemtooltip(Text: string): `
* `ImGui::SetItemTooltip`
#### `imgui.beginchild(Name: string, Width: number? [ 0 ], Height: number? [ 0 ], ChildFlags: string? [ "" ], WindowFlags: string? [ "" ]): boolean`
* `ImGui::BeginChild`
* `ChildFlags` may be a combination of any of the following sequences, optionally separated by ' ' or '|' characters: `b` (borders), `wp` (always use window padding), `rx` (resize X), `ry` (resize Y), `ax` (auto resize X), `ay` (auto resize Y), `f` (frame style)
* `WindowFlags` may be a combination of any of the following sequences, optionally separated by ' ' or '|' characters: `nt` (no titlebar), `nr` (no resize), `nm` (no move), `nc` (no collapse), `ns` (no saved settings), `ni` (no interact), `h` (horizontal scrolling)
#### `imgui.popid(): `
* `ImGui::PopID`
#### `imgui.separatortext(Text: string): `
* `ImGui::SeparatorText`
#### `imgui.urllink(Text: string, Url: string? [ Text ]): boolean`
* Similar to `imgui.textlink`, however, will open the given URL upon being clicked
* `Url` will be `Text` if not provided
* `ImGui::TextLinkOpenUrl`
#### `imgui.pushstylecolor(StyleIndex: number, Color: { number }): `
* `ImGui::PushStyleColor`
* `Color` is an array with 4 numbers representing R, G, B, and A
#### `imgui.beginmenu(Name: string, Enabled: boolean?): boolean`
* `ImGui::BeginMenu`
#### `imgui.pushid(Id: string): `
* `ImGui::PushID`
#### `imgui.popstylecolor(): `
* `ImGui::PopStyleColor`
#### `imgui.setnextitemwidth(Width: number): `
* Sets the width of the next item
* `ImGui::SetNextItemWidth`
#### `imgui.endchild(): `
* `ImGui::EndChild`
#### `imgui.beginpopupmodal(Name: string, HasCloseButton: boolean? [ false ]): boolean`
* `ImGui::BeginPopupModal`
#### `imgui.anyitemactive(): boolean`
* `ImGui::IsAnyItemActive`
#### `imgui.itemclicked(): boolean`
* `ImGui::IsItemClicked`
#### `imgui.beginpopup(Name: string): boolean`
* `ImGui::BeginPopup`
#### `imgui.indent(Indent: number?): `
* `ImGui::Indent`
#### `imgui.closecurrentpopup(): `
* `ImGui::CloseCurrentPopup`
#### `imgui.imagebutton(Name: string, Image: string, Size: vector?): boolean`
* `ImGui::ImageButton`
#### `imgui.beginfullscreen(Name: string, OffsetX: number? [ 0 ], OffsetY: number? [ 0 ]): `
* Creates a Dear ImGui window which takes up the entire screen
#### `imgui.endmainmenubar(): `
* `ImGui::EndMainMenuBar`
#### `imgui.beginmenubar(): boolean`
* `ImGui::BeginMenuBar`
#### `imgui.getcontentregionavail(): number, number`
* `ImGui::GetContentRegionAvail`
### `input`
* Checking player inputs
#### `input.cursorvisible(): boolean`
* Returns whether the mouse cursor is current visible
* To *change* the visibility of the cursor, use `.setcursorvisible`
#### `input.keypressed(Key: number): boolean`
* Returns whether the specified key (as lowercase, e.g. `Enum.Key.A`, `Enum.Key.One`) is currently being pressed
#### `input.setmousegrabbed(Grabbed: boolean): `
* Grabs/ungrabs the mouse cursor
#### `input.guihandledm(): boolean`
* Returns whether Dear ImGui is using the player's mouse inputs
#### `input.setmouseposition(MouseX: number, MouseY: number): `
* Moves the mouse to the given coordinates of the window
#### `input.mouseposition(): vector`
* Returns the current position of the mouse
#### `input.setcursorvisible(Visible: boolean): `
* Sets the visibility of the mouse cursor to the specified value
#### `input.mousedown(Button: number): boolean`
* Returns whether the specified mouse button (`Enum.MouseButton.Left/Middle/Right`) is currently being pressed
#### `input.guihandled(): boolean`
* Returns whether Dear ImGui is using the player's inputs at all
#### `input.mousegrabbed(): boolean`
* Returns whether the mouse is currently restricted to the game window
* To *change* whether the mouse is grabbed, use `.setmousegrabbed`
#### `input.guihandledk(): boolean`
* Returns whether Dear ImGui is using the player's keyboard inputs
### `json`
* Encoding and decoding JSON files
#### `json.encode(Value: any): string`
* Encodes the provided value into a JSON string
#### `json.parse(Json: string): any`
* Decodes the JSON string and returns it as a value
### `mesh`
* Mesh assets
#### `mesh.save(Id: string, SaveTo: string): `
* Saves the mesh data at the provided path to a file
#### `mesh.set(Id: string, Data: { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }): `
* Associates the provided mesh data with the provided path
#### `mesh.get(Id: string): { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }`
* Returns the provided mesh data associated with the provided path
### `model`
* `glTF` models
#### `model.import(Path: string): GameObject & Model`
* Imports the glTF model at the provided path and returns it as a `Model` GameObject
### `scene`
* Scene assets
#### `scene.load(Path: string): { GameObject }?, string?`
* Loads `GameObject`s from the scene file at the provided path, returning a list of the root objects or `nil` and an error message upon failure
#### `scene.save(RootNodes: { GameObject }, Path: string): boolean`
* Saves the list of `GameObject`s to the provided path, returning whether the operation succeeded
## Globals

Additional global variables

#### `_VMNAME: string`
* A non-unique identifier for the current Luau VM
#### `appendlog(...: any): ()`
* Same as `print`, but does not prefix the log message with `[INFO]`
#### `breakpoint(Line: number): ()`
* Set a breakpoint at the given line
#### `defer<A...>(Task: ((A...) -> ()) | thread, SleepFor: number?, ...: A...): ()`
* Schedules the given function or coroutine to be resumed some number of seconds later, or as soon as possible
#### `game: GameObject & DataModel`
* The GameObject acting as the Data Model of the Engine
#### `loadthread(Code: string, ChunkName: string?): ( thread?, string? )`
* Like `loadstring` in *other* runtimes, however does not compromise Global Import optimizations and returns a coroutine instead of a function
* If an error occurs, returns `nil` as the first value and the error message as the second value
#### `loadthreadfromfile(File: string, ChunkName: string?): ( thread?, string? )`
* Similar to `loadthread`, however loads from a file instead of from a string directly
#### `script: GameObject & Script`
* The Script object the current coroutine is running as
* In `require`'d modules, this is `nil`
#### `shellexec(Command: string): string`
* Runs a shell command
#### `sleep(SleepTime: number): ()`
* Yields the thread for the specified number of seconds
#### `workspace: GameObject & Workspace`
* Shorthand for `game.Workspace`

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
* For organization, the members of this type will be on the Components wiki page
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
### `conf`
* Internal Engine configuration state, loaded from file (`phoenix.conf`) upon startup
#### `conf.save(): boolean`
* Save the current state of the configuration to file (`phoenix.conf`), returning whether the file was successfully overwritten
#### `conf.set(Key: string, Value: any): `
* Change a specific value in the configuration
#### `conf.get(Key: string): any`
* Read a specific value from the configuration
### `engine`
* Inspect and modify various parts of the Engine
#### `engine.fullscreen(): boolean`
* Returns whether or not the window is currently fullscreened
* To *set* whether the window is fullscreened, use `.setfullscreen`
#### `engine.windowsize(): number, number`
* Returns the current size, in pixels, of the game window
* To *set* the size of the window, use `.setwindowsize`
#### `engine.unloadtexture(Path: string): `
* Unloads the given texture from the Engine's cache
#### `engine.closevm(VM: string): `
* Closes the Luau VM
#### `engine.toolnames(): { string }`
* Returns a list of valid Engine tools
#### `engine.framerate(): number`
* Returns the current framerate
#### `engine.setwindowsize(Width: number, Height: number): `
* Sets the size of the window to the specified two Width and Height integers
#### `engine.daabbs(Enabled: boolean?): boolean`
* Returns whether AABBs are drawn for all objects with collisions enabled (`.PhysicsCollisions == true`)
* Optionally, the visualization can be enabled/disabled by passing in a boolean argument
#### `engine.showmessagebox(Title: string, Message: string, Buttons: ("ok" | "okcancel" | "yesno" | "yesnocancel")? [ "ok" ], Icon: ("info" | "warning" | "error" | "question")? [ "info" ], DefaultButton: number? [ 1 ]): number`
* Shows a message to the player with the given specification
* The returned number will be the selected option: `0` for Cancel/No, `1` for Ok/Yes, `2` for No in `yesnocancel`
* This is a blocking operation and no scripts or other parts of the Engine run until the message is dismissed
* Avoid using quotes (`'` and `"`) or backticks (`\``) in the `Message`
#### `engine.setmaxframerate(MaxFramerate: number): `
* Sets the maximum framerate the Engine will reach before self-limiting
#### `engine.currentvm(): string`
* Returns the name of the currently-bound VM
#### `engine.resetviewport(): `
* Resets the state of the Viewport
#### `engine.setvsync(VSyncEnabled: boolean): `
* Enables/disables VSync
#### `engine.createvm(Name: string): `
* Creates a new Luau VM with the given name
* The `workspace` and `game` globals of the VM are taken from the currently-bound Datamodel (see `engine.binddatamodel`)
#### `engine.binddatamodel(NewDataModel: GameObject): `
* Switches Data Models to the specified Object
#### `engine.settoolenabled(Tool: string, Enabled: boolean): `
* Enables/disables the given tool
#### `engine.setviewport(PositionX: number, PositionY: number, Width: number, Height: number): `
* Configures the Engine to act as though the Viewport is at the given position with the given size
#### `engine.setexplorerroot(Root: GameObject): `
* Changes the Root of the built-in Explorer's hierarchy view
#### `engine.exit(ExitCode: number? [ 0 ]): `
* Shuts down the Engine and exits the process with the specified Exit Code (or 0 by default)
#### `engine.setfullscreen(Fullscreen: boolean): `
* Enables/disables fullscreen mode
#### `engine.runinvm(VM: string, Code: string, ChunkName: string?): number | false, string`
* Runs the given code in the specified VM
* Returns `false` and an error message upon compilation failure
* Returns a status code otherwise, along with a status message
* A status code of `0` means success
#### `engine.setcurrentvm(VM: string): `
* Makes the given VM "current"
* This causes all Scripts which load afterward to use the specified VM, it does *not* change the VM of any Scripts loaded beforehand
#### `engine.physicstimescale(Timescale: number): `
* Sets the time scale factor of the physics simulation
#### `engine.setexplorerselections(Selections: { GameObject }): `
* Changes the built-in Explorer's Object selections to the specified Objects
#### `engine.getvsync(): boolean`
* Returns whether VSync is currently enabled
#### `engine.toolenabled(Tool: string): boolean`
* Returns whether the given tool is currently enabled
* To *enable* or *disable* the tool, use `.settoolenabled`
#### `engine.explorerselections(): { GameObject }`
* Returns what is currently selected in the Explorer
#### `engine.dwireframes(Enabled: boolean?): boolean`
* Returns whether all visible 3D objects are being rendered with wireframes
* Optionally, the visualization can be enabled/disabled by passing in a boolean argument
#### `engine.isheadless(): boolean`
* Returns whether the Engine is currently running Headless mode
#### `engine.args(): { string }`
* Returns the list of launch arguments given to the Engine
* The first item is always the path to the Engine executable
#### `engine.maxframerate(): number`
* Returns the current maximum framerate setting
* To *set* the maximum framerate, use `.setmaxframerate`
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
#### `imgui.image(ImagePath: string, Size: { number }?, FlipVertically: boolean? [ false ], TintColor: { number }? [ { 1, 1, 1, 1 } ]): `
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
#### `imgui.imagebutton(Name: string, Image: string, Size: { number }?): boolean`
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
#### `input.keypressed(Key: string | number): boolean`
* Returns whether the specified key (as lowercase, e.g. `'a'`, `'b'`, or by GLFW Key ID) is currently being pressed
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
#### `input.mousedown(Button: 'l' | 'r' | 'e'): boolean`
* Returns whether the specified mouse button (`l`eft, `r`ight, or `e`ither) is currently being pressed
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
#### `sleep(SleepTime: number): ()`
* Yields the thread for the specified number of seconds
#### `workspace: GameObject & Workspace`
* Shorthand for `game.Workspace`

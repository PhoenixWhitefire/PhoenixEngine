export type EventSignal<T...> = {
	Connect: (self: EventSignal<T...>, Callback: (T...) -> ()) -> (EventConnection),
	WaitUntil: (self: EventSignal<T...>) -> (T...)
}

declare extern type KeyInputEvent with
	Key: EnumKey
	Scancode: number
	Action: EnumInputAction
	Modifiers: number
end

declare extern type MouseButtonInputEvent with
	Button: EnumMouseButton
	Action: EnumInputAction
	Modifiers: number
end

declare extern type ScrollInputEvent with
	X: number
	Y: number
end

export type JsonSerializable = nil | boolean | number | string | vector | { JsonSerializable } | { [string]: JsonSerializable }

declare extern type Enum with
end

declare extern type EnumItem with
end

type function IntersectCreatables(ty: type)
	if ty:is("singleton") then
		for i, c in Creatables:properties() do
			if i:value() == ty:value() then
				return c.read
			end
		end

		error(`invalid component '{ty:value()}'`)

	elseif ty:is("union") then
		local results = {}

		for _, t in ty:components() do
			local found = false

			for i, c in Creatables:properties() do
				if i:value() == t:value() then
					table.insert(results, c.read)
					found = true
					break
				end
			end

			if not found then
				error(`invalid component '{t:value()}'`)
			end
		end

		return types.intersectionof(table.unpack(results))
	end

	error(`expected singleton or union, got {ty.tag}`)

	return types.never
end

declare extern type __ENGINE_INTERNAL_USERDATA_PHANTOM_TYPE__ with
end
declare extern type Color with
  B: number
  G: number
  R: number
end
declare Color: {
  new: (( R: number, G: number, B: number ) -> (Color)) & (( Color: vector ) -> (Color))
}
declare extern type EventConnection with
  function Disconnect(self): (  )
  Connected: boolean
  Signal: unknown
end
declare GameObject: {
  fromId: (( Id: number ) -> (GameObject?)),
  new: ({ string }) -> (GameObject & any),
  validComponents: { string }
}
declare extern type Matrix with
  function ExtractAngles(self): ( vector )
  function Inverse(self): ( Matrix )
  function Lerp(self, Target: Matrix, Delta: number): ( Matrix )
  function __add(self, Param_1: vector): ( Matrix )
  function __mul(self, Param_1: Matrix | vector): ( Matrix )
  function __sub(self, Param_1: vector): ( Matrix )
  C1R1: number
  C1R2: number
  C1R3: number
  C1R4: number
  C2R1: number
  C2R2: number
  C2R3: number
  C2R4: number
  C3R1: number
  C3R2: number
  C3R3: number
  C3R4: number
  C4R1: number
  C4R2: number
  C4R3: number
  C4R4: number
  Forward: vector
  Position: vector
  Right: vector
  Up: vector
end
declare Matrix: {
  fromAngles: (( X: number, Y: number, Z: number ) -> (Matrix)) & (( Angles: vector ) -> (Matrix)),
  fromEulerAnglesXYZ: (( X: number, Y: number, Z: number ) -> (Matrix)) & (( Angles: vector ) -> (Matrix)),
  fromEulerAnglesYXZ: (( X: number, Y: number, Z: number ) -> (Matrix)) & (( Angles: vector ) -> (Matrix)),
  fromTranslation: (( Position: vector ) -> (Matrix)) & (( X: number, Y: number, Z: number ) -> (Matrix)),
  identity: Matrix,
  lookAt: (( Eye: vector, Target: vector ) -> (Matrix)),
  new: ((  ) -> (Matrix)) & (( X: number, Y: number, Z: number ) -> (Matrix)) & (( Position: vector ) -> (Matrix))
}


export type GameObject = {
  Enabled: boolean,
  Name: string,
  ObjectId: number ,
  Parent: GameObject?,
  Serializes: boolean,

  AddComponent: (self: GameObject, Component: keyof<Creatables>) -> (),
  AddTag: (self: GameObject, Tag: string) -> (),
  Destroy: (self: GameObject) -> (),
  Duplicate: (self: GameObject) -> (GameObject),
  FindChild: (self: GameObject, ChildName: string) -> (GameObject?),
  FindChildWithComponent: <T>(self: GameObject, ComponentName: keyof<Creatables> & T) -> (GameObject & index<Creatables, T>)?,
  ForEachChild: (self: GameObject, Callback: (Object: GameObject) -> boolean?) -> (),
  ForEachDescendant: (self: GameObject, Callback: (Object: GameObject) -> boolean?) -> (),
  GetChildren: (self: GameObject) -> { GameObject },
  GetComponents: (self: GameObject) -> keyof<Creatables>,
  GetDescendants: (self: GameObject) -> { GameObject },
  GetFullName: (self: GameObject) -> (string),
  GetTags: (self: GameObject) -> { string },
  HasComponent: (self: GameObject, Component: keyof<Creatables>) -> (boolean),
  HasTag: (self: GameObject, Tag: string) -> (boolean),
  MergeWith: (self: GameObject, With: GameObject) -> (),
  RemoveComponent: (self: GameObject, Component: keyof<Creatables>) -> (),
  RemoveTag: (self: GameObject, Tag: string) -> (),

  OnTagAdded: EventSignal<(string)>,
  OnTagRemoved: EventSignal<(string)>,

  __phantom__: __ENGINE_INTERNAL_USERDATA_PHANTOM_TYPE__,
}
export type Animation = {
  Animation: string,
  Looped: boolean,
  Playing: boolean,
  Ready: boolean ,
  Weight: number,
}
export type AssetManager = {

  GetMeshData: (self: AssetManager, Id: string) -> { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } },
  ImportModel: (self: AssetManager, Path: string) -> GameObject & Model,
  LoadScene: (self: AssetManager, Path: string) -> ({ GameObject }?, string),
  SaveMesh: (self: AssetManager, Id: string, Path: string) -> (),
  SaveScene: (self: AssetManager, RootNodes: { GameObject }, Path: string) -> (boolean, string?),
  SetMeshData: (self: AssetManager, Id: string, MeshData: { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }) -> (),
}
export type Bone = {
  IsActive: boolean ,
  SkeletalBoneId: number ,
  TargetMesh: GameObject ,
  Transform: Matrix,
}
export type Camera = {
  FieldOfView: number,
  UseSimpleController: boolean,
}
export type Collections = {

  GetCollections: (self: Collections) -> { string },
  GetTagAddedSignal: (self: Collections, Tag: string) -> EventSignal<GameObject>,
  GetTagRemovedSignal: (self: Collections, Tag: string) -> EventSignal<GameObject>,
  GetTagged: (self: Collections, Tag: string) -> { GameObject },
}
export type DataModel = {
  AreScriptsBound: boolean ,
  LiveScripts: string,
  Time: number ,
  VM: string,

  BindToClose: (self: DataModel, Callback: () -> ()) -> (),
  GetService: (self: DataModel, Service: 'Engine' | 'PlayerInput' | 'AssetManager' | 'History' | 'Physics' | 'Renderer' | 'Collections' | 'Network') -> GameObject,

  OnFrameBegin: EventSignal<(number)>,
}
export type DirectionalLight = {
  Brightness: number,
  Direction: vector,
  LightColor: Color,
  ShadowViewDistance: number,
  ShadowViewFarPlane: number,
  ShadowViewMoveWithCamera: boolean,
  ShadowViewNearPlane: number,
  ShadowViewOffset: vector,
  ShadowViewSize: number,
  ShadowViewSizeH: number,
  ShadowViewSizeV: number,
  Shadows: boolean,
}
export type Engine = {
  BoundDataModel: GameObject ,
  CommitHash: string ,
  Framerate: number ,
  FramerateCap: number,
  IsHeadless: boolean ,
  TargetPlatform: string ,
  Version: string ,

  BindDataModel: (self: Engine, DataModel:  GameObject & DataModel ) -> (),
  CloseVM: (self: Engine, VmName: string) -> (),
  CreateVM: (self: Engine, VmName: string) -> (),
  Exit: (self: Engine, ExitCode: number?) -> (),
  GetCliArguments: (self: Engine) -> { string },
  GetConfigValue: (self: Engine, Key: string) -> (JsonSerializable),
  GetExplorerSelections: (self: Engine) -> ({ GameObject }),
  GetToolNames: (self: Engine) -> { string },
  IsToolEnabled: (self: Engine, ToolName: string) -> (boolean),
  OpenTextDocument: (self: Engine, Path: string, Line: number?) -> (),
  ResetViewportInputRect: (self: Engine) -> (),
  RunInVM: (self: Engine, VmName: string, Code: string, ChunkName: string?) -> (boolean, string?),
  SaveConfig: (self: Engine) -> (boolean, string),
  SetConfigValue: (self: Engine, Key: string, Value: JsonSerializable) -> (),
  SetExplorerRoot: (self: Engine, Root: GameObject) -> (),
  SetExplorerSelections: (self: Engine, Selections: { GameObject }) -> (),
  SetToolEnabled: (self: Engine, ToolName: string, Enabled: boolean) -> (),
  SetViewportInputRect: (self: Engine, Position: vector, Size: vector) -> (),
  ShowMessageBox: (self: Engine, Title: string, Message: string, Buttons: 'ok' | 'okcancel' | 'yesno' | 'yesnocancel' | nil, Icon: 'info' | 'warning' | 'error' | 'question' | nil, DefaultButton: number?) -> (number),

  OnMessageLogged: EventSignal<EnumLogType, string, string>,
}
export type History = {
  ActionHistorySize: number ,
  CanRedo: boolean ,
  CanUndo: boolean ,
  CurrentActionName: string? ,
  CurrentWaypoint: number ,
  IsRecordingAction: boolean ,
  TargetDataModel: GameObject,

  ClearHistory: (self: History) -> (),
  DiscardAction: (self: History, Id: number) -> (),
  EnableRecording: (self: History) -> (),
  FinishAction: (self: History, Id: number) -> (),
  GetActionData: (self: History, Index: number) -> { Name: string, Events: { { Target: GameObject, Property: string, NewValue: unknown, PreviousValue: unknown } } },
  GetCannotRedoReason: (self: History) -> (string),
  GetCannotUndoReason: (self: History) -> (string),
  GetCurrentActionData: (self: History) -> { Name: string, Events: { { Target: GameObject, Property: string, NewValue: unknown, PreviousValue: unknown } } }?,
  Redo: (self: History) -> (),
  TryBeginAction: (self: History, Name: string) -> (number?),
  Undo: (self: History) -> (),
}
export type Mesh = {
  CastsShadows: boolean,
  FaceCulling: EnumFaceCulling,
  Material: string,
  MeshAsset: string,
  MetallnessFactor: number,
  RoughnessFactor: number,
  Tint: Color,
  Transparency: number,
}
export type Model = {
}
export type Network = {

  MakeHttpRequestAsync: (self: Network, { Url: string, Method: nil | 'GET' | 'HEAD' | 'POST' | 'PUT' | 'DELETE' | 'PATCH', Headers: { [string]: string }?, Body: string? }) -> ({ Ok: false, Status: number, Error: string?, Headers: { [string]: string }, Body: string } | { Ok: true, Status: number, Headers: { [string]: string }, Body: string }),
}
export type ParticleEmitter = {
  Emitting: boolean,
  Lifetime: vector,
  ParticlesAreAttached: boolean,
  Rate: number,
}
export type Physics = {
  DebugCollisionAabbs: boolean,
  DebugContactPoints: boolean,
  DebugSpatialHeat: boolean,
  Gravity: vector,
  Simulating: boolean,
  Timescale: number,
}
export type PlayerInput = {
  Cursor: EnumCursor,
  CursorMode: EnumCursorMode,
  IsKeyboardSunk: boolean ,
  IsMouseSunk: boolean ,

  GetCursorPosition: (self: PlayerInput) -> (vector),
  IsKeyPressed: (self: PlayerInput, Key: EnumKey) -> (boolean),
  IsMouseButtonPressed: (self: PlayerInput, Button: EnumMouseButton) -> (boolean),

  KeyEvent: EventSignal<KeyInputEvent>,
  MouseButtonEvent: EventSignal<MouseButtonInputEvent>,
  ScrollEvent: EventSignal<ScrollInputEvent>,
}
export type PointLight = {
  Brightness: number,
  LightColor: Color,
  Range: number,
}
export type Renderer = {
  DebugWireframeRendering: boolean,
  IsFullscreen: boolean,
  Resolution: vector,
  VSync: boolean,

  SetShaderVariable: ((_: Renderer, Shader: string, Variable: string, Value: boolean | number | Color | Matrix, Type: EnumValueType?) -> ()) & ((_: Renderer, Shader: string, Variable: string, Value: vector, Type: EnumValueType) -> ()),
  UnloadTexture: (self: Renderer, Texture: string) -> (),
}
export type RigidBody = {
  AngularVelocity: vector,
  CollisionType: EnumCollisionType,
  Density: number,
  Friction: number,
  GravityFactor: number,
  HullsFile: string,
  LinearVelocity: vector,
  PhysicsCollisions: boolean,
  PhysicsDynamics: boolean,
  PhysicsRotations: boolean,
  Restitution: number,
}
export type Sound = {
  FinishedLoading: boolean ,
  Length: number ,
  LoadSucceeded: boolean ,
  Looped: boolean,
  Playing: boolean,
  Position: number,
  SoundFile: string,
  Speed: number,
  Volume: number,

  Play: (self: Sound) -> (),

  OnLoaded: EventSignal<(boolean)>,
}
export type SpotLight = {
  Angle: number,
  Brightness: number,
  LightColor: Color,
  Range: number,
}
export type Transform = {
  LocalSize: vector ,
  LocalTransform: Matrix ,
  Size: vector,
  Transform: Matrix,
}
export type TreeLink = {
  Target: GameObject?,
}
export type Workspace = {
  SceneCamera: GameObject & Camera & Transform,

  GetObjectsInAabb: (self: Workspace, Position: vector, Size: vector, IgnoreList: { GameObject }?) -> { GameObject & RigidBody & Transform },
  Raycast: (self: Workspace, Origin: vector, CastVector: vector, FilterList: { GameObject }?, FilterIsIgnoreList: boolean?) -> { Object: GameObject & RigidBody & Transform, Position: vector, Normal: vector }?,
  ScreenPointToVector: (self: Workspace, ScreenPoint: vector, Length: number?) -> (vector),
  WorldToScreenPoint: (self: Workspace, WorldPoint: vector) -> (vector, number),
}



type Creatables = {
	Transform: Transform,
	Mesh: Mesh & Transform,
	ParticleEmitter: ParticleEmitter & Transform,
	Sound: Sound,
	Workspace: Workspace,
	DataModel: DataModel,
	PointLight: PointLight & Transform,
	DirectionalLight: DirectionalLight,
	SpotLight: SpotLight & Transform,
	Camera: Camera & Transform,
	Animation: Animation,
	Model: Model & Transform,
	Bone: Bone,
	TreeLink: TreeLink,
	Engine: Engine,
	PlayerInput: PlayerInput,
	AssetManager: AssetManager,
	History: History,
	RigidBody: RigidBody,
	Physics: Physics,
	Renderer: Renderer,
	Collections: Collections,
	Network: Network,
}
declare extern type EnumCollisionType extends EnumItem with end
declare extern type EnumCollisionType_INTERNAL extends Enum with
  Cube: EnumCollisionType
  Hulls: EnumCollisionType
  MeshComponent: EnumCollisionType
  Sphere: EnumCollisionType
end
declare extern type EnumCursor extends EnumItem with end
declare extern type EnumCursor_INTERNAL extends Enum with
  Arrow: EnumCursor
  Crosshair: EnumCursor
  NotAllowed: EnumCursor
  PointingHand: EnumCursor
  ResizeAll: EnumCursor
  ResizeHorizontal: EnumCursor
  ResizeNESW: EnumCursor
  ResizeNWSE: EnumCursor
  ResizeVertical: EnumCursor
  TextInput: EnumCursor
end
declare extern type EnumCursorMode extends EnumItem with end
declare extern type EnumCursorMode_INTERNAL extends Enum with
  Captured: EnumCursorMode
  Disabled: EnumCursorMode
  Hidden: EnumCursorMode
  Normal: EnumCursorMode
end
declare extern type EnumFaceCulling extends EnumItem with end
declare extern type EnumFaceCulling_INTERNAL extends Enum with
  Back: EnumFaceCulling
  Front: EnumFaceCulling
  None: EnumFaceCulling
end
declare extern type EnumInputAction extends EnumItem with end
declare extern type EnumInputAction_INTERNAL extends Enum with
  Pressed: EnumInputAction
  Released: EnumInputAction
  Repeated: EnumInputAction
end
declare extern type EnumKey extends EnumItem with end
declare extern type EnumKey_INTERNAL extends Enum with
  A: EnumKey
  Apostrophe: EnumKey
  B: EnumKey
  Backslash: EnumKey
  Backspace: EnumKey
  C: EnumKey
  CapsLock: EnumKey
  Comma: EnumKey
  D: EnumKey
  Delete: EnumKey
  Down: EnumKey
  E: EnumKey
  Eight: EnumKey
  End: EnumKey
  Enter: EnumKey
  Equal: EnumKey
  Escape: EnumKey
  F: EnumKey
  F1: EnumKey
  F10: EnumKey
  F11: EnumKey
  F12: EnumKey
  F13: EnumKey
  F14: EnumKey
  F15: EnumKey
  F16: EnumKey
  F17: EnumKey
  F18: EnumKey
  F19: EnumKey
  F2: EnumKey
  F20: EnumKey
  F21: EnumKey
  F22: EnumKey
  F23: EnumKey
  F24: EnumKey
  F25: EnumKey
  F3: EnumKey
  F4: EnumKey
  F5: EnumKey
  F6: EnumKey
  F7: EnumKey
  F8: EnumKey
  F9: EnumKey
  Five: EnumKey
  Four: EnumKey
  G: EnumKey
  GraveAccent: EnumKey
  H: EnumKey
  Home: EnumKey
  I: EnumKey
  Insert: EnumKey
  J: EnumKey
  K: EnumKey
  L: EnumKey
  Left: EnumKey
  LeftAlt: EnumKey
  LeftBracket: EnumKey
  LeftControl: EnumKey
  LeftShift: EnumKey
  LeftSuper: EnumKey
  M: EnumKey
  Minus: EnumKey
  N: EnumKey
  Nine: EnumKey
  NumLock: EnumKey
  Numpad0: EnumKey
  Numpad1: EnumKey
  Numpad2: EnumKey
  Numpad3: EnumKey
  Numpad4: EnumKey
  Numpad5: EnumKey
  Numpad6: EnumKey
  Numpad7: EnumKey
  Numpad8: EnumKey
  Numpad9: EnumKey
  NumpadAdd: EnumKey
  NumpadDecimal: EnumKey
  NumpadDivide: EnumKey
  NumpadEnter: EnumKey
  NumpadEqual: EnumKey
  NumpadMultiply: EnumKey
  NumpadSubtract: EnumKey
  O: EnumKey
  One: EnumKey
  P: EnumKey
  PageDown: EnumKey
  PageUp: EnumKey
  Pause: EnumKey
  Period: EnumKey
  PrintScreen: EnumKey
  Q: EnumKey
  R: EnumKey
  Right: EnumKey
  RightAlt: EnumKey
  RightBracket: EnumKey
  RightControl: EnumKey
  RightShift: EnumKey
  RightSuper: EnumKey
  S: EnumKey
  ScrollLock: EnumKey
  Semicolon: EnumKey
  Seven: EnumKey
  Six: EnumKey
  Slash: EnumKey
  Space: EnumKey
  T: EnumKey
  Tab: EnumKey
  Three: EnumKey
  Two: EnumKey
  U: EnumKey
  Up: EnumKey
  V: EnumKey
  W: EnumKey
  World1: EnumKey
  World2: EnumKey
  X: EnumKey
  Y: EnumKey
  Z: EnumKey
  Zero: EnumKey
end
declare extern type EnumLogType extends EnumItem with end
declare extern type EnumLogType_INTERNAL extends Enum with
  Error: EnumLogType
  Info: EnumLogType
  None: EnumLogType
  Warning: EnumLogType
end
declare extern type EnumMouseButton extends EnumItem with end
declare extern type EnumMouseButton_INTERNAL extends Enum with
  Left: EnumMouseButton
  Middle: EnumMouseButton
  Right: EnumMouseButton
end
declare extern type EnumValueType extends EnumItem with end
declare extern type EnumValueType_INTERNAL extends Enum with
  Any: EnumValueType
  Array: EnumValueType
  Boolean: EnumValueType
  Color: EnumValueType
  Double: EnumValueType
  EventSignal: EnumValueType
  Function: EnumValueType
  GameObject: EnumValueType
  InputEvent: EnumValueType
  Integer: EnumValueType
  Map: EnumValueType
  Matrix: EnumValueType
  Null: EnumValueType
  String: EnumValueType
  Vector2: EnumValueType
  Vector3: EnumValueType
end
declare Enum: {
  CollisionType: EnumCollisionType_INTERNAL,
  Cursor: EnumCursor_INTERNAL,
  CursorMode: EnumCursorMode_INTERNAL,
  FaceCulling: EnumFaceCulling_INTERNAL,
  InputAction: EnumInputAction_INTERNAL,
  Key: EnumKey_INTERNAL,
  LogType: EnumLogType_INTERNAL,
  MouseButton: EnumMouseButton_INTERNAL,
  ValueType: EnumValueType_INTERNAL,
  [ | "CollisionType" | "Cursor" | "CursorMode" | "FaceCulling" | "InputAction" | "Key" | "LogType" | "MouseButton" | "ValueType"]:  | EnumCollisionType_INTERNAL | EnumCursor_INTERNAL | EnumCursorMode_INTERNAL | EnumFaceCulling_INTERNAL | EnumInputAction_INTERNAL | EnumKey_INTERNAL | EnumLogType_INTERNAL | EnumMouseButton_INTERNAL | EnumValueType_INTERNAL
}

declare debug: {
	traceback: ((msg: string?, level: number?) -> string) & (co: thread, msg: string?, level: number?) -> string,
	info: (level: number, s: string) -> string,
  breakpoint: (( (Line: number ) -> (number)) & (() -> ()))
}
declare fs: {
  copy: (( From: string, To: string ) -> ()),
  cwd: ((  ) -> (string)),
  definealias: (( Alias: string, For: string ) -> ()),
  execute: (( Command: string ) -> (string)),
  isdirectory: (( string ) -> (boolean)),
  isfile: (( string ) -> (boolean)),
  listdir: (( Path: string, Filter: ('a' | 'f' | 'd')? ) -> ({ [string]: 'f' | 'd' })),
  mkdir: (( Path: string ) -> ()),
  promptopen: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string?, AllowMultipleFiles: boolean? ) -> ({ string })),
  promptopenfolder: (( Title: string, DefaultDirectory: string? ) -> (string?)),
  promptsave: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string? ) -> (string?)),
  read: (( Path: string ) -> (string?, string?)),
  remove: (( Path: string ) -> ()),
  rename: (( Path: string, NewName: string ) -> ()),
  resolvepath: (Path: string) -> string,
  resolvepathabsolute: (Path: string) -> string,
  setunqualifiedroot: (( Alias: string ) -> ()),
  write: (( Path: string, Contents: string, CreateDirectories: boolean? ) -> ())
}
declare imgui: {
  anyitemactive: ((  ) -> (boolean)),
  begin: (( WindowTitle: string, WindowFlags: string?, HasCloseButton: boolean? ) -> (boolean, boolean)),
  beginchild: (( Name: string, Width: number?, Height: number?, ChildFlags: string?, WindowFlags: string? ) -> (boolean)),
  beginfullscreen: (( Name: string, OffsetX: number?, OffsetY: number? ) -> ()),
  beginmainmenubar: ((  ) -> (boolean)),
  beginmenu: (( Name: string, Enabled: boolean? ) -> (boolean)),
  beginmenubar: ((  ) -> (boolean)),
  beginpopup: (( Name: string ) -> (boolean)),
  beginpopupmodal: (( Name: string, HasCloseButton: boolean? ) -> (boolean)),
  begintooltip: ((  ) -> (boolean)),
  button: (( Text: string, Width: number?, Height: number? ) -> (boolean)),
  checkbox: (( Name: string, Value: boolean ) -> (boolean)),
  closecurrentpopup: ((  ) -> ()),
  combo: (( Text: string, Options: { string }, CurrentOption: number ) -> (number)),
  contentregionavail: ((  ) -> (vector)),
  cursorposition: ((  ) -> (vector)),
  demowindow: (( HasCloseButton: boolean? ) -> (boolean)),
  dummy: (( Width: number?, Height: number? ) -> ()),
  endchild: ((  ) -> ()),
  endmainmenubar: ((  ) -> ()),
  endmenu: ((  ) -> ()),
  endmenubar: ((  ) -> ()),
  endpopup: ((  ) -> ()),
  endtooltip: ((  ) -> ()),
  endw: ((  ) -> ()),
  image: (( ImagePath: string, Size: vector?, FlipVertically: boolean?, TintColor: { number }?, DoBilinearSmoothing: boolean? ) -> ()),
  imagebutton: (( Name: string, Image: string, Size: vector? ) -> (boolean)),
  indent: (( Indent: number? ) -> ()),
  inputnumber: (( Text: string, Value: number ) -> (number)),
  inputstring: (( Name: string, Value: string, IsPassword: boolean? ) -> (string)),
  itemclicked: ((  ) -> (boolean)),
  itemhovered: ((  ) -> (boolean)),
  menuitem: (( Text: string, Enabled: boolean? ) -> (boolean)),
  newline: ((  ) -> ()),
  openpopup: (( Name: string ) -> ()),
  popid: ((  ) -> ()),
  popstylecolor: ((  ) -> ()),
  pushid: (( Id: string ) -> ()),
  pushstylecolor: (( StyleIndex: number, Color: { number } ) -> ()),
  sameline: ((  ) -> ()),
  scrollherey: ((  ) -> ()),
  separator: ((  ) -> ()),
  separatortext: (( Text: string ) -> ()),
  setcursorposition: ((Position: vector) -> ()) & ((X: number, Y: number) -> ()),
  setitemtooltip: (( Text: string ) -> ()),
  setnextitemwidth: (( Width: number ) -> ()),
  setnextwindowfocus: ((  ) -> ()),
  setnextwindowopen: (( Open: boolean? ) -> ()),
  setnextwindowposition: (( X: number, Y: number ) -> ()),
  setnextwindowsize: (( Width: number, Height: number ) -> ()),
  settooltip: (( Tooltip: string ) -> ()),
  setviewportdockspace: (( PositionX: number, PositionY: number, SizeX: number, SizeY: number ) -> ()),
  setviewportdockspacedefault: ((  ) -> ()),
  stylecolors: (( Theme: 'l' | 'd' ) -> ()),
  text: (( Text: string ) -> ()),
  textlink: (( Text: string ) -> (boolean)),
  textsize: (( Text: string ) -> (vector)),
  treenode: (( Text: string, Flags: string? ) -> (boolean)),
  treepop: ((  ) -> ()),
  urllink: (( Text: string, Url: string? ) -> (boolean)),
  windowhovered: (( Flags: string? ) -> (boolean)),
  windowposition: ((  ) -> (vector)),
  windowsize: ((  ) -> (vector))
}
declare json: {
  encode: (( Value: JsonSerializable, Indent: number? ) -> (string)),
  parse: (( Json: string ) -> (JsonSerializable))
}
declare task: {
  defer: (<A...>( Task: ((A...) -> ()) | thread, A... ) -> ()),
  delay: (<A...>( Delay: number, Task: ((A...) -> ()) | thread, A... ) -> ()),
  load: (( Code: string, ChunkName: string? ) -> (thread?, string?)),
  loadfile: (( File: string, ChunkName: string? ) -> (thread?, string?)),
  wait: (( SleepTime: number? ) -> (number))
}


declare _VMNAME: string
declare function appendlog( ...: unknown ): ()
declare game: GameObject & DataModel
declare function warn( ...: unknown ): ()
declare workspace: GameObject & Workspace



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
  new: (<T>(ComponentName: T | keyof<Creatables>) -> GameObject & index<Creatables, T>),
  validComponents: { string }
}
declare extern type Matrix with
  function ExtractAngles(self): ( vector )
  function Inverse(self): ( Matrix )
  function Lerp(self, Target: Matrix, Delta: number): ( Matrix )
  function __mul(self, Param_1: Matrix): ( Matrix )
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


declare extern type GameObject with
  Enabled: boolean
  Name: string
  ObjectId: number 
  Parent: GameObject?
  Serializes: boolean

  function AddComponent(self, Component: keyof<Creatables>) : ()
  function AddTag(self, Tag: string) : ()
  function Destroy(self) : ()
  function Duplicate(self) : (GameObject)
  function FindChild(self, ChildName: string) : (GameObject?)
  function FindChildWithComponent(self, Component: keyof<Creatables>) : (GameObject)
  function ForEachChild(self, Callback: (...unknown) -> (...unknown)) : ()
  function ForEachDescendant(self, Callback: (...unknown) -> (...unknown)) : ()
  function GetChildren(self) : { GameObject }
  function GetComponents(self) : keyof<Creatables>
  function GetDescendants(self) : { GameObject }
  function GetFullName(self) : (string)
  function GetTags(self) : { string }
  function HasComponent(self, Component: keyof<Creatables>) : (boolean)
  function HasTag(self, Tag: string) : (boolean)
  function MergeWith(self, With: GameObject) : ()
  function RemoveComponent(self, Component: keyof<Creatables>) : ()
  function RemoveTag(self, Tag: string) : ()

  OnTagAdded: EventSignal<(string)>
  OnTagRemoved: EventSignal<(string)>
end
declare extern type Animation with
  Animation: string
  Looped: boolean
  Playing: boolean
  Ready: boolean 
  Weight: number
end
declare extern type AssetManager with

  function GetMeshData(self, Id: string) : { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }
  function ImportModel(self, Path: string) : GameObject & Model
  function LoadScene(self, Path: string) : ({ GameObject }?, string)
  function SaveMesh(self, Id: string, Path: string) : ()
  function SaveScene(self, RootNodes: { GameObject }, Path: string) : (boolean, string?)
  function SetMeshData(self, Id: string, MeshData: { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }) : ()
end
declare extern type Bone with
  IsActive: boolean 
  SkeletalBoneId: number 
  TargetMesh: GameObject 
  Transform: Matrix
end
declare extern type Camera with
  FieldOfView: number
  UseSimpleController: boolean
end
declare extern type Collections with

  function GetCollections(self) : { string }
  function GetTagAddedSignal(self, Tag: string) : EventSignal<GameObject>
  function GetTagRemovedSignal(self, Tag: string) : EventSignal<GameObject>
  function GetTagged(self, Tag: string) : { GameObject }
end
declare extern type DataModel with
  AreScriptsBound: boolean 
  LiveScripts: string
  Time: number 
  VM: string

  function BindToClose(self, Callback: () -> ()) : ()
  function GetService(self, Service: 'Workspace' | 'Engine' | 'PlayerInput' | 'AssetManager' | 'History' | 'Physics' | 'Renderer' | 'Collections') : GameObject

  OnFrameBegin: EventSignal<(number)>
end
declare extern type DirectionalLight with
  Brightness: number
  Direction: vector
  LightColor: Color
  ShadowViewDistance: number
  ShadowViewFarPlane: number
  ShadowViewMoveWithCamera: boolean
  ShadowViewNearPlane: number
  ShadowViewOffset: vector
  ShadowViewSize: number
  ShadowViewSizeH: number
  ShadowViewSizeV: number
  Shadows: boolean
end
declare extern type Engine with
  BoundDataModel: GameObject 
  CommitHash: string 
  Framerate: number 
  FramerateCap: number
  IsHeadless: boolean 
  Version: string 

  function BindDataModel(self, DataModel:  GameObject & DataModel ) : ()
  function CloseVM(self, VmName: string) : ()
  function CreateVM(self, VmName: string) : ()
  function Exit(self, ExitCode: number?) : ()
  function GetCliArguments(self) : { string }
  function GetConfigValue(self, Key: string) : (JsonSerializable)
  function GetExplorerSelections(self) : ({ GameObject })
  function GetToolNames(self) : { string }
  function IsToolEnabled(self, ToolName: string) : (boolean)
  function OpenTextDocument(self, Path: string, Line: number?) : ()
  function ResetViewportInputRect(self) : ()
  function RunInVM(self, VmName: string, Code: string, ChunkName: string?) : (boolean, string?)
  function SaveConfig(self) : (boolean, string)
  function SetConfigValue(self, Key: string, Value: JsonSerializable) : ()
  function SetExplorerRoot(self, Root: GameObject) : ()
  function SetExplorerSelections(self, Selections: { GameObject }) : ()
  function SetToolEnabled(self, ToolName: string, Enabled: boolean) : ()
  function SetViewportInputRect(self, Position: vector, Size: vector) : ()
  function ShowMessageBox(self, Title: string, Message: string, Buttons: 'ok' | 'okcancel' | 'yesno' | 'yesnocancel' | nil, Icon: 'info' | 'warning' | 'error' | 'question' | nil, DefaultButton: number?) : (number)

  OnMessageLogged: EventSignal<EnumLogType, string, string>
end
declare extern type History with
  ActionHistorySize: number 
  CanRedo: boolean 
  CanUndo: boolean 
  CurrentActionName: string? 
  CurrentWaypoint: number 
  IsRecordingAction: boolean 
  TargetDataModel: GameObject

  function ClearHistory(self) : ()
  function DiscardAction(self, Id: number) : ()
  function EnableRecording(self) : ()
  function FinishAction(self, Id: number) : ()
  function GetActionData(self, Index: number) : { Name: string, Events: { { Target: GameObject, Property: string, NewValue: unknown, PreviousValue: unknown } } }
  function GetCannotRedoReason(self) : (string)
  function GetCannotUndoReason(self) : (string)
  function GetCurrentActionData(self) : { Name: string, Events: { { Target: GameObject, Property: string, NewValue: unknown, PreviousValue: unknown } } }?
  function Redo(self) : ()
  function TryBeginAction(self, Name: string) : (number?)
  function Undo(self) : ()
end
declare extern type Mesh with
  CastsShadows: boolean
  FaceCulling: EnumFaceCulling
  Material: string
  MeshAsset: string
  MetallnessFactor: number
  RoughnessFactor: number
  Tint: Color
  Transparency: number
end
declare extern type Model with
end
declare extern type ParticleEmitter with
  Emitting: boolean
  Lifetime: vector
  ParticlesAreAttached: boolean
  Rate: number
end
declare extern type Physics with
  DebugCollisionAabbs: boolean
  DebugContactPoints: boolean
  DebugSpatialHeat: boolean
  Gravity: vector
  Simulating: boolean
  Timescale: number
end
declare extern type PlayerInput with
  Cursor: EnumCursor
  CursorMode: EnumCursorMode
  IsKeyboardSunk: boolean 
  IsMouseSunk: boolean 

  function GetCursorPosition(self) : (vector)
  function IsKeyPressed(self, Key: EnumKey) : (boolean)
  function IsMouseButtonPressed(self, Button: EnumMouseButton) : (boolean)

  KeyEvent: EventSignal<KeyInputEvent>
  MouseButtonEvent: EventSignal<MouseButtonInputEvent>
  ScrollEvent: EventSignal<ScrollInputEvent>
end
declare extern type PointLight with
  Brightness: number
  LightColor: Color
  Range: number
end
declare extern type Renderer with
  DebugWireframeRendering: boolean
  IsFullscreen: boolean
  Resolution: vector
  VSync: boolean

  SetShaderVariable: ((_: Renderer, Shader: string, Variable: string, Value: boolean | number | Color | Matrix, Type: EnumValueType?) -> ()) & ((_: Renderer, Shader: string, Variable: string, Value: vector, Type: EnumValueType) -> ())  function UnloadTexture(self, Texture: string) : ()
end
declare extern type RigidBody with
  AngularVelocity: vector
  CollisionType: EnumCollisionType
  Density: number
  Friction: number
  GravityFactor: number
  HullsFile: string
  LinearVelocity: vector
  PhysicsCollisions: boolean
  PhysicsDynamics: boolean
  PhysicsRotations: boolean
  Restitution: number
end
declare extern type Sound with
  FinishedLoading: boolean 
  Length: number 
  LoadSucceeded: boolean 
  Looped: boolean
  Playing: boolean
  Position: number
  SoundFile: string
  Speed: number
  Volume: number

  function Play(self) : ()

  OnLoaded: EventSignal<(boolean)>
end
declare extern type SpotLight with
  Angle: number
  Brightness: number
  LightColor: Color
  Range: number
end
declare extern type Transform with
  LocalSize: vector 
  LocalTransform: Matrix 
  Size: vector
  Transform: Matrix
end
declare extern type TreeLink with
  Target: GameObject?
end
declare extern type Workspace with
  SceneCamera: GameObject & Camera

  function GetObjectsInAabb(self, Position: vector, Size: vector, IgnoreList: { GameObject }?) : { GameObject & RigidBody & Transform }
  function Raycast(self, Origin: vector, CastVector: vector, FilterList: { GameObject }?, FilterIsIgnoreList: boolean?) : { Object: GameObject & RigidBody & Transform, Position: vector, Normal: vector }?
  function ScreenPointToVector(self, ScreenPoint: vector, Length: number?) : (vector)
end



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
	Camera: Camera,
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
  endw: ((  ) -> ()),
  image: (( ImagePath: string, Size: vector?, FlipVertically: boolean?, TintColor: { number }?, DoBilinearSmoothing: boolean? ) -> ()),
  imagebutton: (( Name: string, Image: string, Size: vector? ) -> (boolean)),
  indent: (( Indent: number? ) -> ()),
  inputnumber: (( Text: string, Value: number ) -> (number)),
  inputstring: (( Name: string, Value: string ) -> (string)),
  itemclicked: ((  ) -> (boolean)),
  itemhovered: ((  ) -> (boolean)),
  menuitem: (( Text: string, Enabled: boolean? ) -> (boolean)),
  openpopup: (( Name: string ) -> ()),
  popid: ((  ) -> ()),
  popstylecolor: ((  ) -> ()),
  pushid: (( Id: string ) -> ()),
  pushstylecolor: (( StyleIndex: number, Color: { number } ) -> ()),
  sameline: ((  ) -> ()),
  scrollherey: ((  ) -> ()),
  separator: ((  ) -> ()),
  separatortext: (( Text: string ) -> ()),
  setcursorposition: (( X: number, Y: number ) -> ()),
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



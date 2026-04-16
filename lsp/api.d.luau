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

type function IntersectComponents(ty: type)
	if ty:is("singleton") then
		for i, c in Components:properties() do
			if i:value() == ty:value() then
				return c.read
			end
		end

		error(`invalid component '{ty:value()}'`)

	elseif ty:is("union") then
		local results = {}

		for _, t in ty:components() do
			local found = false

			for i, c in Components:properties() do
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

declare function ______leakgenericshack<T>(_: T): T

export type HistoryEvent = {
	Type: "Component",
	Action: "Add" | "Remove",
	Target: GameObject,
	Component: string,
}
| {
	Type: "Tag",
	Action: "Add" | "Remove",
	Target: GameObject,
	Tag: string,
}
| {
	Type: "PropertyChanged",
	Target: GameObject,
	Property: string,
	NewValue: unknown,
	PreviousValue: unknown
}

export type HistoryAction = {
	Name: string,
	Events: { HistoryEvent }
}

export type MeshAssetData = {
	Vertices: {
		{
			Position: vector,
			Normal: vector,
			Paint: { number },
			UV: vector
		}
	},
	Indices: { number }
}

type Services = {
  Engine: Engine,
  PlayerInput: PlayerInput,
  AssetManager: AssetManager,
  History: History,
  Physics: Physics,
  Renderer: Renderer,
  Collections: Collections,
  Network: Network,
  Logging: Logging,
  Interface: Interface,
  Environment: Environment,
  DeveloperTools: DeveloperTools,
}

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
  fromComponents: (<T>(Components: { T & keyof<Components> }) -> GameObject & IntersectComponents<T>),
  fromId: (( Id: number ) -> (GameObject?)),
  new: (<T>(MainComponent: T & keyof<Creatables>) -> GameObject & index<Creatables, T>),
  validComponents: { string }
}
declare extern type Matrix with
  function ExtractAngles(self): ( vector )
  function Inverse(self): ( Matrix )
  function Lerp(self, Target: Matrix, Delta: number): ( Matrix )
  function __add(self, Param_1: vector): ( Matrix )
  __mul: ((self: Matrix, b: Matrix) -> Matrix) & ((self: Matrix, v: vector) -> vector)
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
  Duplicate: <T>(self: GameObject & T) -> (GameObject & T),
  FindChild: (self: GameObject, ChildName: string) -> (GameObject?),
  FindChildWithComponent: (self: GameObject, Component: keyof<Creatables>) -> (GameObject),
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
}
export type Animation = {
  Animation: string,
  Looped: boolean,
  Playing: boolean,
  Ready: boolean ,
  Weight: number,
}
export type AssetManager = {

  GetMeshData: (self: AssetManager, Id: string) -> MeshAssetData,
  ImportModel: (self: AssetManager, Path: string) -> GameObject & Model,
  LoadScene: (self: AssetManager, Path: string) -> ({ GameObject }?, string),
  QueueLoadTexture: (self: AssetManager, Path: string, BilinearFiltering: boolean, ImGui: boolean) -> (),
  SaveMesh: (self: AssetManager, Id: string, Path: string) -> (),
  SaveScene: (self: AssetManager, RootNodes: { GameObject }, Path: string) -> (boolean, string?),
  SetMeshData: (self: AssetManager, Id: string, MeshData: MeshAssetData | buffer) -> (),
  UnloadMaterial: (self: AssetManager, Material: string) -> (),
  UnloadMesh: (self: AssetManager, Mesh: string) -> (),
  UnloadTexture: (self: AssetManager, Texture: string) -> (),
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
  GetService: <T>(self: DataModel, Service: keyof<Services> & T) -> GameObject & index<Services, T>,

  OnFrameBegin: EventSignal<(number)>,
}
export type DeveloperTools = {
  DocumentationShown: boolean,
  ExplorerShown: boolean,
  InfoShown: boolean,
  Initialized: boolean ,
  MaterialsShown: boolean,
  PropertiesShown: boolean,
  RendererShown: boolean,
  ScriptsShown: boolean,
  SettingsShown: boolean,
  ShadersShown: boolean,

  GetExplorerSelections: (self: DeveloperTools) -> ({ GameObject }),
  GetToolNames: (self: DeveloperTools) -> { string },
  Initialize: (self: DeveloperTools) -> (),
  IsToolEnabled: (self: DeveloperTools, ToolName: string) -> (boolean),
  OpenTextDocument: (self: DeveloperTools, Path: string, Line: number?) -> (),
  SetExplorerRoot: (self: DeveloperTools, Root: GameObject) -> (),
  SetExplorerSelections: (self: DeveloperTools, Selections: { GameObject }) -> (),
  SetToolEnabled: (self: DeveloperTools, ToolName: string, Enabled: boolean) -> (),
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
  RunInVM: (self: Engine, VmName: string, Code: string, ChunkName: string?) -> (boolean, string?),
  SaveConfig: (self: Engine) -> (boolean, string),
  SetConfigValue: (self: Engine, Key: string, Value: JsonSerializable) -> (),
  ShowMessageBox: (self: Engine, Title: string, Message: string, Buttons: 'ok' | 'okcancel' | 'yesno' | 'yesnocancel' | nil, Icon: 'info' | 'warning' | 'error' | 'question' | nil, DefaultButton: number?) -> (number),
}
export type Environment = {
  AmbientLight: Color,
  Fog: boolean,
  FogColor: Color,
  GammaCorrection: number,
  PostProcess: boolean,
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
  GetActionData: (self: History, Index: number) -> HistoryAction,
  GetCannotRedoReason: (self: History) -> (string),
  GetCannotUndoReason: (self: History) -> (string),
  GetCurrentActionData: (self: History) -> HistoryAction?,
  Redo: (self: History) -> (),
  TryBeginAction: (self: History, Name: string) -> (number?),
  Undo: (self: History) -> (),
}
export type Interface = {
}
export type Logging = {

  Write: (self: Logging, Message: string, Type: EnumLogType, ExtraTags: string?) -> (),

  OnMessaged: EventSignal<EnumLogType, string, string, unknown>,
}
export type Mesh = {
  CastsShadows: boolean,
  FaceCulling: EnumFaceCulling,
  Material: string,
  MeshAsset: string,
  MetalnessFactor: number,
  RoughnessFactor: number,
  Tint: Color,
  Transparency: number,
}
export type Model = {
  ImportPath: string,
}
export type Network = {

  MakeHttpRequestAsync: (self: Network, { Url: string, Method: nil | 'GET' | 'HEAD' | 'POST' | 'PUT' | 'DELETE' | 'PATCH', Headers: { [string]: string }?, Body: string? }) -> ({ Ok: boolean, Status: number, Headers: { [string]: string }, Body: string }),
}
export type ParticleEmitter = {
  BilinearFiltering: boolean,
  Emitting: boolean,
  Image: string,
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
  ResetViewportInputRect: (self: PlayerInput) -> (),
  SetViewportInputRect: (self: PlayerInput, Position: vector, Size: vector) -> (),

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

  SetShaderTextureVariable: (self: Renderer, Shader: string, Variable: string, TexturePath: string, BilinearFiltering: boolean, InLinearColorSpace: boolean) -> (),
  SetShaderVariable: ((_: Renderer, Shader: string, Variable: string, Value: boolean | number | Color | Matrix, Type: EnumValueType?) -> ()) & ((_: Renderer, Shader: string, Variable: string, Value: vector, Type: EnumValueType) -> ()),
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
  LocalSize: vector,
  LocalTransform: Matrix,
  Size: vector ,
  Transform: Matrix ,
}
export type TreeLink = {
  Target: GameObject?,
}
export type UIButton = {

  OnClicked: EventSignal<()>,
}
export type UIFrame = {
  BackgroundColor: Color,
  BackgroundTransparency: number,
}
export type UIImage = {
  Image: string,
  ImageTint: Color,
  ImageTransparency: number,
}
export type UIText = {
  Text: string,
}
export type UITransform = {
  Position: vector,
  Rotation: number,
  Size: vector,
  ZIndex: number,
}
export type Workspace = {
  SceneCamera: GameObject & Camera & Transform,

  GetObjectsInAabb: (self: Workspace, Position: vector, Size: vector, IgnoreList: { GameObject }?) -> { GameObject & RigidBody & Transform },
  Raycast: (self: Workspace, Origin: vector, CastVector: vector, FilterList: { GameObject }?, FilterIsIgnoreList: boolean?) -> { Object: GameObject & RigidBody & Transform, Position: vector, Normal: vector }?,
  ScreenPointToVector: (self: Workspace, ScreenPoint: vector, Length: number?) -> (vector),
  WorldToScreenPoint: (self: Workspace, WorldPoint: vector) -> (vector, number),
}



type Components = {
  Transform: Transform,
  Mesh: Mesh,
  ParticleEmitter: ParticleEmitter,
  Sound: Sound,
  Workspace: Workspace,
  DataModel: DataModel,
  PointLight: PointLight,
  DirectionalLight: DirectionalLight,
  SpotLight: SpotLight,
  Camera: Camera,
  Animation: Animation,
  Model: Model,
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
  Logging: Logging,
  Interface: Interface,
  UITransform: UITransform,
  UIFrame: UIFrame,
  UIImage: UIImage,
  UIText: UIText,
  UIButton: UIButton,
  Environment: Environment,
  DeveloperTools: DeveloperTools,
}
type Creatables = {
  Transform: Transform,
  Mesh: Mesh & RigidBody & Transform,
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
  RigidBody: RigidBody & Transform,
  Physics: Physics,
  Renderer: Renderer,
  Collections: Collections,
  Network: Network,
  Logging: Logging,
  Interface: Interface,
  UITransform: UITransform,
  UIFrame: UIFrame,
  UIImage: UIImage,
  UIText: UIText,
  UIButton: UIButton,
  Environment: Environment,
  DeveloperTools: DeveloperTools,
}
declare extern type EnumCollisionType extends EnumItem with end
declare extern type EnumCollisionType_INTERNAL extends Enum with
  Cube: EnumCollisionType
  Sphere: EnumCollisionType
  Hulls: EnumCollisionType
  MeshComponent: EnumCollisionType
end
declare extern type EnumCursor extends EnumItem with end
declare extern type EnumCursor_INTERNAL extends Enum with
  Arrow: EnumCursor
  TextInput: EnumCursor
  Crosshair: EnumCursor
  PointingHand: EnumCursor
  ResizeHorizontal: EnumCursor
  ResizeVertical: EnumCursor
  ResizeNWSE: EnumCursor
  ResizeNESW: EnumCursor
  ResizeAll: EnumCursor
  NotAllowed: EnumCursor
end
declare extern type EnumCursorMode extends EnumItem with end
declare extern type EnumCursorMode_INTERNAL extends Enum with
  Normal: EnumCursorMode
  Hidden: EnumCursorMode
  Disabled: EnumCursorMode
  Captured: EnumCursorMode
end
declare extern type EnumFaceCulling extends EnumItem with end
declare extern type EnumFaceCulling_INTERNAL extends Enum with
  None: EnumFaceCulling
  Back: EnumFaceCulling
  Front: EnumFaceCulling
end
declare extern type EnumFileSystemEvent extends EnumItem with end
declare extern type EnumFileSystemEvent_INTERNAL extends Enum with
  Added: EnumFileSystemEvent
  Removed: EnumFileSystemEvent
  Modified: EnumFileSystemEvent
  Renamed_Old: EnumFileSystemEvent
  Renamed_New: EnumFileSystemEvent
end
declare extern type EnumInputAction extends EnumItem with end
declare extern type EnumInputAction_INTERNAL extends Enum with
  Released: EnumInputAction
  Pressed: EnumInputAction
  Repeated: EnumInputAction
end
declare extern type EnumKey extends EnumItem with end
declare extern type EnumKey_INTERNAL extends Enum with
  Space: EnumKey
  Apostrophe: EnumKey
  Comma: EnumKey
  Minus: EnumKey
  Period: EnumKey
  Slash: EnumKey
  Zero: EnumKey
  One: EnumKey
  Two: EnumKey
  Three: EnumKey
  Four: EnumKey
  Five: EnumKey
  Six: EnumKey
  Seven: EnumKey
  Eight: EnumKey
  Nine: EnumKey
  Semicolon: EnumKey
  Equal: EnumKey
  A: EnumKey
  B: EnumKey
  C: EnumKey
  D: EnumKey
  E: EnumKey
  F: EnumKey
  G: EnumKey
  H: EnumKey
  I: EnumKey
  J: EnumKey
  K: EnumKey
  L: EnumKey
  M: EnumKey
  N: EnumKey
  O: EnumKey
  P: EnumKey
  Q: EnumKey
  R: EnumKey
  S: EnumKey
  T: EnumKey
  U: EnumKey
  V: EnumKey
  W: EnumKey
  X: EnumKey
  Y: EnumKey
  Z: EnumKey
  LeftBracket: EnumKey
  Backslash: EnumKey
  RightBracket: EnumKey
  GraveAccent: EnumKey
  World1: EnumKey
  World2: EnumKey
  Escape: EnumKey
  Enter: EnumKey
  Tab: EnumKey
  Backspace: EnumKey
  Insert: EnumKey
  Delete: EnumKey
  Right: EnumKey
  Left: EnumKey
  Down: EnumKey
  Up: EnumKey
  PageUp: EnumKey
  PageDown: EnumKey
  Home: EnumKey
  End: EnumKey
  CapsLock: EnumKey
  ScrollLock: EnumKey
  NumLock: EnumKey
  PrintScreen: EnumKey
  Pause: EnumKey
  F1: EnumKey
  F2: EnumKey
  F3: EnumKey
  F4: EnumKey
  F5: EnumKey
  F6: EnumKey
  F7: EnumKey
  F8: EnumKey
  F9: EnumKey
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
  F20: EnumKey
  F21: EnumKey
  F22: EnumKey
  F23: EnumKey
  F24: EnumKey
  F25: EnumKey
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
  NumpadDecimal: EnumKey
  NumpadDivide: EnumKey
  NumpadMultiply: EnumKey
  NumpadSubtract: EnumKey
  NumpadAdd: EnumKey
  NumpadEnter: EnumKey
  NumpadEqual: EnumKey
  LeftShift: EnumKey
  LeftControl: EnumKey
  LeftAlt: EnumKey
  LeftSuper: EnumKey
  RightShift: EnumKey
  RightControl: EnumKey
  RightAlt: EnumKey
  RightSuper: EnumKey
end
declare extern type EnumLogType extends EnumItem with end
declare extern type EnumLogType_INTERNAL extends Enum with
  None: EnumLogType
  Info: EnumLogType
  Warning: EnumLogType
  Error: EnumLogType
end
declare extern type EnumMouseButton extends EnumItem with end
declare extern type EnumMouseButton_INTERNAL extends Enum with
  Left: EnumMouseButton
  Right: EnumMouseButton
  Middle: EnumMouseButton
end
declare extern type EnumValueType extends EnumItem with end
declare extern type EnumValueType_INTERNAL extends Enum with
  Boolean: EnumValueType
  Integer: EnumValueType
  Double: EnumValueType
  String: EnumValueType
  Buffer: EnumValueType
  Color: EnumValueType
  Vector2: EnumValueType
  Vector3: EnumValueType
  Matrix: EnumValueType
  GameObject: EnumValueType
  Function: EnumValueType
  Array: EnumValueType
  Map: EnumValueType
  EventSignal: EnumValueType
  InputEvent: EnumValueType
  NumberGradient: EnumValueType
  VectorGradient: EnumValueType
  ColorGradient: EnumValueType
  Null: EnumValueType
  Any: EnumValueType
end
declare Enum: {
  CollisionType: EnumCollisionType_INTERNAL,
  Cursor: EnumCursor_INTERNAL,
  CursorMode: EnumCursorMode_INTERNAL,
  FaceCulling: EnumFaceCulling_INTERNAL,
  FileSystemEvent: EnumFileSystemEvent_INTERNAL,
  InputAction: EnumInputAction_INTERNAL,
  Key: EnumKey_INTERNAL,
  LogType: EnumLogType_INTERNAL,
  MouseButton: EnumMouseButton_INTERNAL,
  ValueType: EnumValueType_INTERNAL,
  [ | "CollisionType" | "Cursor" | "CursorMode" | "FaceCulling" | "FileSystemEvent" | "InputAction" | "Key" | "LogType" | "MouseButton" | "ValueType"]:  | EnumCollisionType_INTERNAL | EnumCursor_INTERNAL | EnumCursorMode_INTERNAL | EnumFaceCulling_INTERNAL | EnumFileSystemEvent_INTERNAL | EnumInputAction_INTERNAL | EnumKey_INTERNAL | EnumLogType_INTERNAL | EnumMouseButton_INTERNAL | EnumValueType_INTERNAL
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
  lastwritten: (( Path: string ) -> (number)),
  listdir: (( Path: string, Filter: ('a' | 'f' | 'd')? ) -> ({ [string]: 'f' | 'd' })),
  mkdir: (( Path: string ) -> ()),
  move: (( CurrentPath: string, NewPath: string ) -> ()),
  promptopen: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string?, AllowMultipleFiles: boolean? ) -> ({ string })),
  promptopenfolder: (( Title: string, DefaultDirectory: string? ) -> (string?)),
  promptsave: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string? ) -> (string?)),
  read: (( Path: string ) -> (string?, string?)),
  remove: (( Path: string ) -> ()),
  removealias: (( Alias: string ) -> ()),
  rename: (( Path: string, NewName: string ) -> ()),
  resolvepath: (Path: string) -> string,
  resolvepathabsolute: (Path: string) -> string,
  setunqualifiedroot: (( Alias: string ) -> ()),
  watch: (( Path: string, Callback: (Path: string, Event: EnumFileSystemEvent) -> () ) -> (() -> ())),
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
  focuskeyboardhere: (( Offset: number? ) -> ()),
  image: (( ImagePath: string, Size: vector?, FlipVertically: boolean?, TintColor: { number }?, DoBilinearSmoothing: boolean? ) -> ()),
  imagebutton: (( Name: string, Image: string, Size: vector? ) -> (boolean)),
  indent: (( Indent: number? ) -> ()),
  inputnumber: (( Text: string, Value: number ) -> (number)),
  inputstring: (( Name: string, Value: string, IsPassword: boolean? ) -> (string)),
  itemactive: ((  ) -> (boolean)),
  itemclicked: (( EnumMouseButton? ) -> (boolean)),
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
  setnextitemopen: (( Open: boolean? ) -> ()),
  setnextitemwidth: (( Width: number ) -> ()),
  setnextwindowfocus: ((  ) -> ()),
  setnextwindowopen: (( Open: boolean? ) -> ()),
  setnextwindowposition: (( X: number, Y: number ) -> ()),
  setnextwindowsize: (( Width: number, Height: number ) -> ()),
  setstylecolor: (( StyleIndex: number, Color: { number } ) -> ()),
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
  _pfork: <A..., R...>(fn: (A...) -> (R...), callback: ((R...) -> ())?, A...) -> boolean,
  defer: (<A...>( Task: ((A...) -> ()) | thread, A... ) -> ()),
  delay: (<A...>( Delay: number, Task: ((A...) -> ()) | thread, A... ) -> ()),
  load: (( Code: string, ChunkName: string? ) -> (thread?, string?)),
  loadfile: (( File: string, ChunkName: string? ) -> (thread?, string?)),
  wait: (( SleepTime: number? ) -> (number))
}


declare _VMNAME: string
declare function appendlog<T...>( ...: T... ): ()
declare game: GameObject & DataModel
declare function warn<T...>( ...: T... ): ()
declare workspace: GameObject & Workspace



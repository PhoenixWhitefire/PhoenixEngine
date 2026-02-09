export type EventSignal<T...> = {
	Connect: (self: EventSignal<T...>, Callback: (T...) -> ()) -> (EventConnection),
	WaitUntil: (self: EventSignal<T...>) -> (T...)
}

declare extern type KeyInputEvent with
	Key: index<typeof(Enum.Key), keyof<typeof(Enum.Key)>>
	Scancode: number
	Action: index<typeof(Enum.InputAction), keyof<typeof(Enum.InputAction)>>
	Modifiers: number
end

declare extern type MouseButtonInputEvent with
	Button: index<typeof(Enum.MouseButton), keyof<typeof(Enum.MouseButton)>>
	Action: index<typeof(Enum.InputAction), keyof<typeof(Enum.InputAction)>>
	Modifiers: number
end

declare extern type ScrollInputEvent with
	X: number
	Y: number
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
  Signal: any
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
  Exists: boolean 
  Name: string
  ObjectId: number 
  Parent: GameObject?
  Serializes: boolean

  function AddComponent(self, Component: keyof<Creatables>) : ()
  function AddTag(self, Tag: string) : ()
  function Destroy(self) : ()
  function Duplicate(self) : (GameObject)
  function FindChild(self, ChildName: string) : (GameObject?)
  function FindChildWithComponent(self, Component: keyof<Creatables>) : (GameObject & any)
  function ForEachChild(self, Callback: (any) -> (any)) : ()
  function ForEachDescendant(self, Callback: (any) -> (any)) : ()
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
  DebugWireframeRendering: boolean
  Framerate: number 
  FramerateCap: number
  IsFullscreen: boolean
  IsHeadless: boolean 
  VSync: boolean
  Version: string 
  WindowSize: vector

  function BindDataModel(self, DataModel:  GameObject & DataModel ) : ()
  function CloseVM(self, VmName: string) : ()
  function CreateVM(self, VmName: string) : ()
  function Exit(self, ExitCode: number?) : ()
  function GetCliArguments(self) : { string }
  function GetConfigValue(self, Key: string) : (unknown)
  function GetCurrentVM(self) : (string)
  function GetExplorerSelections(self) : ({ GameObject })
  function GetToolNames(self) : { string }
  function IsToolEnabled(self, ToolName: string) : (boolean)
  function OpenTextDocument(self, Path: string, Line: number?) : ()
  function PollEvents(self) : ()
  function ResetViewport(self) : ()
  function RunInVM(self, VmName: string, Code: string, ChunkName: string?) : (boolean, string?)
  function SaveConfig(self) : (boolean, string)
  function SetConfigValue(self, Key: string, Value: unknown) : ()
  function SetCurrentVM(self, VmName: string) : ()
  function SetExplorerRoot(self, Root: GameObject) : ()
  function SetExplorerSelections(self, Selections: { GameObject }) : ()
  function SetToolEnabled(self, ToolName: string, Enabled: boolean) : ()
  function SetViewport(self, Position: vector, Size: vector) : ()
  function ShowMessageBox(self, Title: string, Message: string, Buttons: string?, Icon: string?, DefaultButton: number?) : (number)
  function UnloadTexture(self, Texture: string) : ()

  OnMessageLogged: EventSignal<(number, string, string)>
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
  FaceCulling: number
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
  Cursor: number
  CursorMode: number
  IsKeyboardSunk: boolean 
  IsMouseSunk: boolean 

  function GetCursorPosition(self) : (vector)
  function IsKeyPressed(self, Key: number) : (boolean)
  function IsMouseButtonPressed(self, MouseButton: number) : (boolean)

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
end
declare extern type RigidBody with
  AngularVelocity: vector
  CollisionType: number
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
declare Enum: {
  CollisionType: { Cube: number, Sphere: number, Hulls: number, MeshComponent: number },
  Cursor: { Arrow: number, TextInput: number, ResizeAll: number, PointingHand: number, Crosshair: number, ResizeHorizontal: number, ResizeVertical: number, ResizeNWSE: number, ResizeNESW: number, ResizeAll: number, NotAllowed: number },
  CursorMode: { Normal: number, Hidden: number, Disabled: number, Unavailable: number, Captured: number },
  FaceCulling: { None: number, Front: number, Back: number },
  InputAction: { Release: number, Press: number, Repeat: number },
  Key: { [string]: number },
  LogType: { None: number, Info: number, Warning: number, Error: number },
  MouseButton: { Left: number, Middle: number, Right: number }
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
  promptsave: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string? ) -> (string)),
  read: (( Path: string ) -> (string?, string?)),
  remove: (( Path: string ) -> ()),
  rename: (( Path: string, NewName: string ) -> ()),
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
  encode: (( Value: any, Indent: number? ) -> (string)),
  parse: (( Json: string ) -> (any))
}
declare task: {
  defer: (<A...>( Task: ((A...) -> ()) | thread, A... ) -> ()),
  delay: (<A...>( Delay: number, Task: ((A...) -> ()) | thread, A... ) -> ()),
  load: (( Code: string, ChunkName: string? ) -> (thread?, string?)),
  loadfile: (( File: string, ChunkName: string? ) -> (thread?, string?)),
  wait: (( SleepTime: number? ) -> (number))
}


declare _VMNAME: string
declare function appendlog( ...: any ): ()
declare game: GameObject & DataModel
declare function warn( ...: any ): ()
declare workspace: GameObject & Workspace



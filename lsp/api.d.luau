export type EventSignal<T...> = {
	Connect: (self: EventSignal<T...>, Callback: (T...) -> ()) -> (),
	WaitUntil: (self: EventSignal<T...>) -> (T...)
}
declare extern type Color with
  B: number
  G: number
  R: number
end
declare Color: {
  new: (( R: number, G: number, B: number ) -> (Color))
}
declare extern type EventConnection with
  function Disconnect(self): ( nil )
  Connected: boolean
  Signal: any
end
declare GameObject: {
  fromId: (( Id: number ) -> (GameObject?)),
  new: (<T>(ComponentName: T | keyof<Creatables>) -> GameObject & index<Creatables, T>),
  validcomponents: { string }
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
  Parent: (GameObject & any)?
  Serializes: boolean

  function AddComponent(self, Component: keyof<Creatables>) : ()
  function Destroy(self) : ()
  function Duplicate(self) : ((GameObject & any))
  function FindChild(self, ChildName: string) : ((GameObject & any)?)
  function FindChildWithComponent(self, Component: keyof<Creatables>) : (GameObject & any)
  function ForEachChild(self, Param_1: (any) -> (any)) : ()
  function ForEachDescendant(self, Param_1: (any) -> (any)) : ()
  function GetChildren(self) : ({ any })
  function GetComponents(self) : (keyof<Creatables>)
  function GetDescendants(self) : ({ any })
  function GetFullName(self) : (string)
  function HasComponent(self, Component: keyof<Creatables>) : (boolean)
  function MergeWith(self, Param_1: (GameObject & any)) : ()
  function RemoveComponent(self, Component: keyof<Creatables>) : ()
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
  function SaveMesh(self, Id: string, Path: string) : ()
  function SetMeshData(self, Id: string, MeshData: { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }) : ()
end
declare extern type Bone with
  IsActive: boolean 
  SkeletalBoneId: number 
  TargetMesh: (GameObject & any) 
  Transform: Matrix
end
declare extern type Camera with
  FieldOfView: number
  UseSimpleController: boolean
end
declare extern type DataModel with
  IsModuleBound: boolean 
  Module: string
  Time: number 

  function GetService(self, ServiceName: string) : ((GameObject & any))

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
  BoundDataModel: (GameObject & any) 
  CommitHash: string 
  DebugCollisionAabbs: boolean
  DebugSpatialHeat: boolean
  DebugWireframeRendering: boolean
  Framerate: number 
  FramerateCap: number
  IsFullscreen: boolean
  IsHeadless: boolean 
  PhysicsTimescale: number
  VSync: boolean
  Version: string 
  WindowSize: vector

  function BindDataModel(self, DataModel:  GameObject & DataModel ) : ()
  function CloseVM(self, VmName: string) : ()
  function CreateVM(self, VmName: string) : ()
  function Exit(self, ExitCode: number?) : ()
  function GetCliArguments(self) : ({ any })
  function GetCurrentVM(self) : (string)
  function GetExplorerSelections(self) : ({ GameObject })
  function GetToolNames(self) : { string }
  function IsToolEnabled(self, ToolName: string) : (boolean)
  function OpenTextDocument(self, Path: string, Line: number?) : ()
  function ResetViewport(self) : ()
  function RunInVM(self, VmName: string, Code: string, ChunkName: string?) : (boolean, string?)
  function SetCurrentVM(self, VmName: string) : ()
  function SetExplorerRoot(self, Root: (GameObject & any)) : ()
  function SetExplorerSelections(self, Selections: { GameObject }) : ()
  function SetToolEnabled(self, ToolName: string, Enabled: boolean) : ()
  function SetViewport(self, Position: vector, Size: vector) : ()
  function ShowMessageBox(self, Title: string, Message: string, Buttons: string?, Icon: string?, DefaultButton: number?) : (number)
  function UnloadTexture(self, Texture: string) : ()

  OnMessageLogged: EventSignal<(number, string, string)>
end
declare extern type Example with
  EvenMoreSecretMessage: string 
  SecretMessage: string 
  SomeInteger: number
  SuperCoolBool: boolean
  WhereIAm: vector

  function GiveUp(self) : ()
  function Greet(self, Names: { string }, Villain: boolean) : string

  OnGreeted: EventSignal<(string, string)>
end
declare extern type History with
  ActionHistorySize: number 
  CurrentActionName: string? 
  CurrentWaypoint: number 
  IsRecordingAction: boolean 
  TargetDataModel: (GameObject & any)

  function CanRedo(self) : (boolean)
  function CanUndo(self) : (boolean)
  function ClearHistory(self) : ()
  function DiscardCurrentAction(self) : ()
  function EnableRecording(self) : ()
  function FinishCurrentAction(self) : ()
  function GetActionData(self, Param_1: number) : ({ [string]: any })
  function GetCurrentActionData(self) : ({ [string]: any }?)
  function Redo(self) : ()
  function TryBeginAction(self, Param_1: string) : (boolean)
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
declare extern type PlayerInput with
  CursorMode: number
  IsKeyboardSunk: boolean 
  IsMouseSunk: boolean 

  function GetCursorPosition(self) : (vector)
  function IsKeyPressed(self, Key: number) : (boolean)
  function IsMouseButtonPressed(self, MouseButton: number) : (boolean)
end
declare extern type PointLight with
  Brightness: number
  LightColor: Color
  Range: number
end
declare extern type RigidBody with
  AngularVelocity: vector
  CollisionFidelity: number
  Density: number
  Friction: number
  LinearVelocity: vector
  PhysicsCollisions: boolean
  PhysicsDynamics: boolean
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
  Target: (GameObject & any)?
end
declare extern type Workspace with
  SceneCamera: GameObject & Camera

  function GetObjectsInAabb(self, Position: vector, Size: vector, IgnoreList: { any }?) : { GameObject }
  function Raycast(self, Origin: vector, CastVector: vector, FilterList: { any }?, FilterIsIgnoreList: boolean?) : { Object: GameObject & Mesh & Transform, Position: vector, Normal: vector }?
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
	Example: Example,
	TreeLink: TreeLink,
	Engine: Engine,
	PlayerInput: PlayerInput,
	AssetManager: AssetManager,
	History: History,
	RigidBody: RigidBody,
}
declare Enum: {
  CollisionFidelity: { Aabb: number, AabbStaticSize: number },
  CursorMode: { Normal: number, Hidden: number, Disabled: number, Unavailable: number, Captured: number },
  FaceCulling: { None: number, Front: number, Back: number },
  Key: { [string]: number },
  LogType: { None: number, Info: number, Warning: number, Error: number },
  MouseButton: { Left: number, Middle: number, Right: number }
}
declare conf: {
  get: (( Key: string ) -> (any)),
  save: ((  ) -> (boolean)),
  set: (( Key: string, Value: any ) -> ())
}
declare debug: {
	traceback: (msg: string?, level: number?) -> string,
	info: (level: number, s: string) -> string,
  breakpoint: (( (Line: number ) -> (number)) & (() -> ()))
}
declare fs: {
  copy: (( From: string, To: string ) -> (boolean, string?)),
  cwd: ((  ) -> (string)),
  definealias: (( Alias: string, For: string ) -> ()),
  isdirectory: (( string ) -> (boolean)),
  isfile: (( string ) -> (boolean)),
  listdir: (( Path: string, Filter: ('a' | 'f' | 'd')? ) -> ({ [string]: 'f' | 'd' })),
  makecwdaliasof: (( Alias: string ) -> ()),
  mkdir: (( Path: string ) -> (boolean, string?)),
  promptopen: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string?, AllowMultipleFiles: boolean? ) -> ({ string })),
  promptopenfolder: (( Title: string, DefaultDirectory: string? ) -> (string?)),
  promptsave: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string? ) -> (string)),
  read: (( Path: string ) -> (string?, string?)),
  remove: (( Path: string ) -> (number | false, string?)),
  rename: (( Path: string, NewName: string ) -> (boolean, string?)),
  write: (( Path: string, Contents: string, CreateDirectories: boolean? ) -> (boolean, string?))
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
  treenode: (( Text: string ) -> (boolean)),
  treepop: ((  ) -> ()),
  urllink: (( Text: string, Url: string? ) -> (boolean)),
  windowhovered: (( Flags: string? ) -> (boolean)),
  windowposition: ((  ) -> (vector)),
  windowsize: ((  ) -> (vector))
}
declare json: {
  encode: (( Value: any ) -> (string)),
  parse: (( Json: string ) -> (any))
}
declare scene: {
  load: (( Path: string ) -> ({ GameObject }?, string?)),
  save: (( RootNodes: { GameObject }, Path: string ) -> (boolean))
}
declare task: {
  defer: (<A...>( Task: ((A...) -> ()) | thread, A... ) -> ()),
  delay: (<A...>( Delay: number, Task: ((A...) -> ()) | thread, A... ) -> ()),
  load: (( Code: string, ChunkName: string? ) -> (thread?, string?)),
  loadfile: (( File: string, ChunkName: string? ) -> (thread?, string?)),
  wait: (( SleepTime: number ) -> (number))
}


declare _VMNAME: string
declare function appendlog( ...: any ): ()
declare game: GameObject & DataModel
declare function shellexec( Command: string ): string
declare function warn( ...: any ): ()
declare workspace: GameObject & Workspace



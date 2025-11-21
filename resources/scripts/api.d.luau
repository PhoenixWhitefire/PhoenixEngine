declare extern type Vector2 with
end
declare extern type Color with
  R: number
  G: number
  B: number
end
declare Color: {
  new: (( R: number, G: number, B: number ) -> (Color))
}
declare extern type EventConnection with
  Connected: boolean
  function Disconnect(self): ( nil )
  Signal: any
end
declare extern type EventSignal with
  function Connect(self, Param_1: (any) -> (any)): ( EventConnection )
  function WaitUntil(self): ( any )
end
declare GameObject: {
  validComponents: { string },
  new: (<T>(ComponentName: T) -> GameObject & index<Creatables, T>) & ((ComponentName: string) -> GameObject & any)
}
declare extern type Matrix with
  function __mul(self, Param_1: Matrix): ( Matrix )
  Right: vector
  C1R1: number
  C4R1: number
  C3R1: number
  C2R1: number
  C1R2: number
  C3R2: number
  C4R2: number
  Up: vector
  C4R3: number
  C3R3: number
  C2R3: number
  C1R3: number
  C2R2: number
  Forward: vector
  Position: vector
  C1R4: number
  C2R4: number
  C3R4: number
  C4R4: number
end
declare Matrix: {
  fromTranslation: (( Position: vector ) -> (Matrix)) & (( X: number, Y: number, Z: number ) -> (Matrix)),
  new: ((  ) -> (Matrix)),
  lookAt: (( Eye: vector, Target: vector ) -> (Matrix)),
  identity: Matrix,
  fromEulerAnglesXYZ: (( X: number, Y: number, Z: number ) -> (Matrix))
}


declare extern type GameObject with
  Enabled: boolean
  Exists: boolean 
  Name: string
  ObjectId: number 
  Parent: (GameObject & any)?
  Serializes: boolean

  function Destroy(self) : ()
  function Duplicate(self) : ((GameObject & any))
  function FindChild(self, Param_1: string) : ((GameObject & any)?)
  function FindChildWithComponent(self, Param_1: string) : ((GameObject & any)?)
  function ForEachChild(self, Param_1: (any) -> (any)) : ()
  function GetChildren(self) : ({ any })
  function GetComponents(self) : (keyof<Creatables>)
  function GetDescendants(self) : ({ any })
  function GetFullName(self) : (string)
  function HasComponent(self, ComponentName: keyof<Creatables> | string) : (boolean)
  function MergeWith(self, Param_1: (GameObject & any)) : ()
end
declare extern type Animation with
  Animation: string
  Looped: boolean
  Playing: boolean
  Ready: boolean 
  Weight: number
end
declare extern type Bone with
  IsActive: boolean 
  SkeletalBoneId: number 
  Transform: Matrix
end
declare extern type Camera with
  FieldOfView: number
  Transform: Matrix
  UseSimpleController: boolean
end
declare extern type DataModel with
  Time: number 

  OnFrameBegin: EventSignal
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
declare extern type Example with
  EvenMoreSecretMessage: string 
  SecretMessage: string 
  SomeInteger: number
  SuperCoolBool: boolean
  WhereIAm: vector

  function GiveUp(self) : ()
  function Greet(self, Param_1: { any }, Param_2: boolean) : (string, string)

  OnGreeted: EventSignal
end
declare extern type Mesh with
  AngularVelocity: vector
  Asset: string
  CastsShadows: boolean
  CollisionFidelity: number
  Density: number
  FaceCulling: number
  Friction: number
  LinearVelocity: vector
  Material: string
  MetallnessFactor: number
  PhysicsCollisions: boolean
  PhysicsDynamics: boolean
  RoughnessFactor: number
  Tint: Color
  Transparency: number
end
declare extern type Model with
end
declare extern type ParticleEmitter with
  Emitting: boolean
  Lifetime: Vector2
  ParticlesAreAttached: boolean
  Rate: number
end
declare extern type PointLight with
  Brightness: number
  LightColor: Color
  Range: number
  Shadows: boolean
end
declare extern type Script with
  SourceFile: string

  function Reload(self) : (boolean)
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

  OnLoaded: EventSignal
end
declare extern type SpotLight with
  Angle: number
  Brightness: number
  LightColor: Color
  Range: number
  Shadows: boolean
end
declare extern type Transform with
  LocalSize: vector 
  LocalTransform: Matrix 
  Size: vector
  Transform: Matrix
end
declare extern type TreeLink with
  Scripting: boolean
  Target: (GameObject & any)?
end
declare extern type Workspace with
  SceneCamera: (GameObject & Camera)?

  function GetObjectsInAabb(self, Param_1: vector, Param_2: vector, Param_3: { any }?) : ({ any })
  function Raycast(self, Origin: vector, CastVector: vector, FilterList: { any }?, FilterIsIgnoreList: boolean?) : ({ [string]: any }?)
  function ScreenPointToRay(self, Param_1: number, Param_2: number, Param_3: number?) : (vector)
end



type Creatables = {
	Transform: Transform,
	Mesh: Mesh & Transform,
	ParticleEmitter: ParticleEmitter & Transform,
	Script: Script,
	Sound: Sound,
	Workspace: Workspace,
	DataModel: DataModel,
	PointLight: PointLight & Transform,
	DirectionalLight: DirectionalLight& Transform,
	SpotLight: SpotLight & Transform,
	Camera: Camera,
	Animation: Animation,
	Model: Model & Transform,
	Bone: Bone,
	Example: Example,
	TreeLink: TreeLink,
}
declare conf: {
  save: ((  ) -> (boolean)),
  set: (( Key: string, Value: any ) -> ()),
  get: (( Key: string ) -> (any))
}
declare engine: {
  fullscreen: ((  ) -> (boolean)),
  windowsize: ((  ) -> (number, number)),
  unloadtexture: (( Path: string ) -> ()),
  pushlvm: (( VmName: string ) -> ()),
  daabbs: (( Enabled: boolean? ) -> (boolean)),
  showmessagebox: (( Title: string, Message: string, Buttons: ("ok" | "okcancel" | "yesno" | "yesnocancel")?, Icon: ("info" | "warning" | "error" | "question")?, DefaultButton: number? ) -> (number)),
  explorerselections: ((  ) -> ({ GameObject })),
  setwindowsize: (( Width: number, Height: number ) -> ()),
  resetviewport: ((  ) -> ()),
  setvsync: (( VSyncEnabled: boolean ) -> ()),
  toolnames: ((  ) -> ({ string })),
  binddatamodel: (( NewDataModel: GameObject ) -> ()),
  settoolenabled: (( Tool: string, Enabled: boolean ) -> ()),
  setviewport: (( PositionX: number, PositionY: number, Width: number, Height: number ) -> ()),
  exit: (( ExitCode: number? ) -> ()),
  poplvm: ((  ) -> ()),
  setfullscreen: (( Fullscreen: boolean ) -> ()),
  setexplorerroot: (( Root: GameObject ) -> ()),
  setmaxframerate: (( MaxFramerate: number ) -> ()),
  physicstimescale: (( Timescale: number ) -> ()),
  setexplorerselections: (( Selections: { GameObject } ) -> ()),
  getvsync: ((  ) -> (boolean)),
  toolenabled: (( Tool: string ) -> (boolean)),
  args: ((  ) -> ({ string })),
  dwireframes: (( Enabled: boolean? ) -> (boolean)),
  isheadless: ((  ) -> (boolean)),
  framerate: ((  ) -> (number)),
  maxframerate: ((  ) -> (number))
}
declare fs: {
  makecwdaliasof: (( Alias: string ) -> ()),
  write: (( Path: string, Contents: string, CreateDirectories: boolean? ) -> (boolean, string?)),
  isfile: (( string ) -> (boolean)),
  promptopenfolder: (( Title: string, DefaultDirectory: string? ) -> (string?)),
  rename: (( Path: string, NewName: string ) -> (boolean, string?)),
  remove: (( Path: string ) -> (number | false, string?)),
  listdir: (( Path: string, Filter: ('a' | 'f' | 'd')? ) -> ({ [string]: 'f' | 'd' })),
  read: (( Path: string ) -> (string?, string?)),
  copy: (( From: string, To: string ) -> (boolean, string?)),
  mkdir: (( Path: string ) -> (boolean, string?)),
  promptopen: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string?, AllowMultipleFiles: boolean? ) -> ({ string })),
  promptsave: (( DefaultLocation: string?, Filter: ( { string } | string)?, FilterName: string? ) -> (string)),
  definealias: (( Alias: string, For: string ) -> ()),
  isdirectory: (( string ) -> (boolean)),
  cwd: ((  ) -> (string))
}
declare imgui: {
  setcursorposition: (( X: number, Y: number ) -> ()),
  beginmainmenubar: ((  ) -> (boolean)),
  endmenubar: ((  ) -> ()),
  dummy: (( Width: number?, Height: number? ) -> ()),
  image: (( ImagePath: string, Size: { number }?, FlipVertically: boolean?, TintColor: { number }? ) -> ()),
  openpopup: (( Name: string ) -> ()),
  settooltip: (( Tooltip: string ) -> ()),
  button: (( Text: string, Width: number?, Height: number? ) -> (boolean)),
  menuitem: (( Text: string, Enabled: boolean? ) -> (boolean)),
  inputstring: (( Name: string, Value: string ) -> (string)),
  checkbox: (( Name: string, Value: boolean ) -> (boolean)),
  text: (( Text: string ) -> ()),
  windowposition: ((  ) -> (number, number)),
  setnextwindowsize: (( Width: number, Height: number ) -> ()),
  endmenu: ((  ) -> ()),
  setviewportdockspace: (( PositionX: number, PositionY: number, SizeX: number, SizeY: number ) -> ()),
  windowhovered: ((  ) -> (boolean)),
  sameline: ((  ) -> ()),
  begin: (( WindowTitle: string, WindowFlags: string? ) -> (boolean)),
  inputnumber: (( Text: string, Value: number ) -> (number)),
  treepop: ((  ) -> ()),
  separator: ((  ) -> ()),
  treenode: (( Text: string ) -> (boolean)),
  setnextwindowopen: (( Open: boolean? ) -> ()),
  endpopup: ((  ) -> ()),
  textsize: (( Text: string ) -> (number, number)),
  textlink: (( Text: string ) -> (boolean)),
  endw: ((  ) -> ()),
  windowsize: ((  ) -> (number, number)),
  combo: (( Text: string, Options: { string }, CurrentOption: number ) -> (number)),
  cursorposition: ((  ) -> (number, number)),
  stylecolors: (( Theme: 'l' | 'd' ) -> ()),
  itemhovered: ((  ) -> (boolean)),
  setnextwindowposition: (( X: number, Y: number ) -> ()),
  setnextwindowfocus: ((  ) -> ()),
  setviewportdockspacedefault: ((  ) -> ()),
  setitemtooltip: (( Text: string ) -> ()),
  beginchild: (( Name: string, Width: number?, Height: number?, ChildFlags: string?, WindowFlags: string? ) -> (boolean)),
  popid: ((  ) -> ()),
  separatortext: (( Text: string ) -> ()),
  urllink: (( Text: string, Url: string? ) -> (boolean)),
  pushstylecolor: (( StyleIndex: number, Color: { number } ) -> ()),
  beginmenu: (( Name: string, Enabled: boolean? ) -> (boolean)),
  pushid: (( Id: string ) -> ()),
  popstylecolor: ((  ) -> ()),
  setnextitemwidth: (( Width: number ) -> ()),
  endchild: ((  ) -> ()),
  beginpopupmodal: (( Name: string, HasCloseButton: boolean? ) -> (boolean)),
  anyitemactive: ((  ) -> (boolean)),
  itemclicked: ((  ) -> (boolean)),
  beginpopup: (( Name: string ) -> (boolean)),
  indent: (( Indent: number? ) -> ()),
  closecurrentpopup: ((  ) -> ()),
  imagebutton: (( Name: string, Image: string, Size: { number }? ) -> (boolean)),
  beginfullscreen: (( Name: string, OffsetX: number?, OffsetY: number? ) -> ()),
  endmainmenubar: ((  ) -> ()),
  beginmenubar: ((  ) -> (boolean)),
  getcontentregionavail: ((  ) -> (number, number))
}
declare input: {
  cursorvisible: ((  ) -> (boolean)),
  keypressed: (( Key: string | number ) -> (boolean)),
  setmousegrabbed: (( Grabbed: boolean ) -> ()),
  guihandledm: ((  ) -> (boolean)),
  setmouseposition: (( MouseX: number, MouseY: number ) -> ()),
  mouseposition: ((  ) -> (number, number)),
  setcursorvisible: (( Visible: boolean ) -> ()),
  mousedown: (( Button: 'l' | 'r' | 'e' ) -> (boolean)),
  guihandled: ((  ) -> (boolean)),
  mousegrabbed: ((  ) -> (boolean)),
  guihandledk: ((  ) -> (boolean))
}
declare json: {
  encode: (( Value: any ) -> (string)),
  parse: (( Json: string ) -> (any))
}
declare mesh: {
  save: (( Id: string, SaveTo: string ) -> ()),
  set: (( Id: string, Data: { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } } ) -> ()),
  get: (( Id: string ) -> ({ Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }))
}
declare model: {
  import: (( Path: string ) -> (GameObject & Model))
}
declare scene: {
  load: (( Path: string ) -> ({ GameObject }?, string?)),
  save: (( RootNodes: { GameObject }, Path: string ) -> (boolean))
}


declare _VMNAME: string
declare function appendlog( ...: any ): ()
declare function breakpoint( Line: number ): ()
declare function defer<A...>( Task: ((A...) -> ()) | thread, SleepFor: number?, ...: A... ): ()
declare game: GameObject & DataModel
declare function loadthread( Code: string, ChunkName: string? ): ( thread?, string? )
declare function loadthreadfromfile( File: string, ChunkName: string? ): ( thread?, string? )
declare script: GameObject & Script
declare function sleep( SleepTime: number ): ()
declare workspace: GameObject & Workspace



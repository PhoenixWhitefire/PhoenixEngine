declare class Vector2
end
declare class Color
  R: number
  G: number
  B: number
end
declare Color: {
  new: (( R: number, G: number, B: number ) -> ())
}
declare class EventConnection
  Connected: boolean
  function Disconnect(self): ( nil )
  Signal: any
end
declare class EventSignal
  function Connect(self, Param_1: (any) -> (any)): ( EventConnection )
end
declare GameObject: {
  validComponents: { string },
  new: (( Component: string ) -> (GameObject))
}
declare class Matrix
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
  new: ((  ) -> (Matrix)),
  fromTranslation: (( Position: vector ) -> (Matrix)) & (( X: number, Y: number, Z: number ) -> (Matrix)),
  lookAt: (( Eye: vector, Target: vector ) -> (Matrix)),
  fromEulerAnglesXYZ: (( X: number, Y: number, Z: number ) -> (Matrix))
}


declare class GameObject
  Enabled: boolean
  Exists: boolean 
  Name: string
  ObjectId: number 
  Parent: GameObject?
  Serializes: boolean

  function Destroy(self)
  function Duplicate(self)
  function FindChild(self, Param_1: string) : (GameObject?)
  function ForEachChild(self, Param_1: (any) -> (any)) : ()
  function GetChildren(self)
  function GetComponentNames(self)
  function GetDescendants(self)
  function GetFullName(self)
  function HasComponent(self, Param_1: string) : (boolean)
  function IsA(self, Param_1: string) : (boolean)
  function MergeWith(self, Param_1: GameObject) : ()
end
declare class Animation
  Animation: string
  Looped: boolean
  Playing: boolean
  Ready: boolean 
  Weight: number
end
declare class Bone
  IsActive: boolean 
  SkeletalBoneId: number 
  Transform: Matrix
end
declare class Camera
  FieldOfView: number
  Transform: Matrix
  UseSimpleController: boolean
end
declare class DataModel
  Time: number 

  OnFrameBegin: EventSignal
end
declare class DirectionalLight
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
declare class Example
  EvenMoreSecretMessage: string 
  SecretMessage: string 
  SomeInteger: number
  SuperCoolBool: boolean
  WhereIAm: vector

  function GiveUp(self)
  function Greet(self, Param_1: { any }, Param_2: boolean) : (string, string)

  OnGreeted: EventSignal
end
declare class Mesh
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
declare class Model
end
declare class ParticleEmitter
  Emitting: boolean
  Lifetime: Vector2
  ParticlesAreAttached: boolean
  Rate: number
end
declare class PointLight
  Brightness: number
  LightColor: Color
  Range: number
  Shadows: boolean
end
declare class Script
  SourceFile: string

  function Reload(self)
end
declare class Sound
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
declare class SpotLight
  Angle: number
  Brightness: number
  LightColor: Color
  Range: number
  Shadows: boolean
end
declare class Transform
  Size: vector
  Transform: Matrix
end
declare class TreeLink
  Scripting: boolean
  Target: GameObject?
end
declare class Workspace
  SceneCamera: GameObject

  function ScreenPointToRay(self, Param_1: { any }, Param_2: number) : (vector)
end


declare conf: {
  save: ((  ) -> (boolean)),
  set: (( Key: string, Value: any ) -> ()),
  get: (( Key: string ) -> (any))
}
declare engine: {
  poplvm: ((  ) -> ()),
  gettoolnames: ((  ) -> ({ string })),
  framerate: ((  ) -> (number)),
  showmessagebox: (( Title: string, Message: string, Type: ('' | 'i' | 'w' | 'e')? ) -> ()),
  setvsync: (( VSyncEnabled: boolean ) -> ()),
  setwindowsize: (( Width: number, Height: number ) -> ()),
  binddatamodel: (( NewDataModel: GameObject ) -> ()),
  settoolenabled: (( Tool: string, Enabled: boolean ) -> ()),
  pushlvm: ((  ) -> ()),
  exit: (( ExitCode: number? ) -> ()),
  setexplorerselections: (( Selections: { GameObject } ) -> ()),
  setfullscreen: (( Fullscreen: boolean ) -> ()),
  setexplorerroot: (( Root: GameObject ) -> ()),
  setmaxframerate: (( MaxFramerate: number ) -> ()),
  istoolenabled: (( Tool: string ) -> (boolean)),
  getfullscreen: ((  ) -> (boolean)),
  getvsync: ((  ) -> (boolean)),
  getmaxframerate: ((  ) -> (number)),
  getwindowsize: ((  ) -> (number, number)),
  dwireframes: (( Enabled: boolean? ) -> (boolean)),
  isheadless: ((  ) -> (boolean)),
  physicstimescale: (( Timescale: number ) -> ()),
  daabbs: (( Enabled: boolean? ) -> (boolean))
}
declare fs: {
  write: (( Path: string, Contents: string ) -> (boolean)),
  read: (( Path: string ) -> (string?)),
  promptopen: (( DefaultLocation: string?, Filter: string?, FilterName: string?, AllowMultipleFiles: boolean? ) -> ({ string })),
  isfile: ((  ) -> (boolean)),
  promptsave: (( DefaultLocation: string?, Filter: string?, FilterName: string? ) -> (string)),
  listdir: (( Path: string, Filter: ('a' | 'f' | 'd')? ) -> ({ [string]: 'f' | 'd' })),
  isdirectory: ((  ) -> (boolean)),
  ispromptactive: ((  ) -> (boolean))
}
declare imgui: {
  beginmainmenubar: ((  ) -> (boolean)),
  endmenubar: ((  ) -> ()),
  dummy: (( Width: number, Height: number ) -> ()),
  image: (( ImagePath: string, Size: { number }?, FlipVertically: boolean?, TintColor: { number }? ) -> ()),
  settooltip: (( Tooltip: string ) -> ()),
  button: (( Text: string, Width: number?, Height: number? ) -> (boolean)),
  menuitem: (( Text: string, Enabled: boolean? ) -> (boolean)),
  inputstring: (( Name: string, Value: string ) -> (string)),
  checkbox: (( Name: string, Value: boolean ) -> (boolean)),
  setnextwindowsize: (( Width: number, Height: number ) -> ()),
  endmenu: ((  ) -> ()),
  text: (( Text: string ) -> ()),
  getcursorpos: ((  ) -> (number, number)),
  begin: (( WindowTitle: string, WindowFlags: string? ) -> (boolean)),
  inputnumber: (( Text: string, Value: number ) -> (number)),
  getwindowsize: ((  ) -> (number, number)),
  setnextwindowopen: (( Open: boolean? ) -> ()),
  sameline: ((  ) -> ()),
  endw: ((  ) -> ()),
  combo: (( Text: string, Options: { string }, CurrentOption: number ) -> (number)),
  windowhovered: ((  ) -> (boolean)),
  treepop: ((  ) -> ()),
  treenode: (( Text: string ) -> (boolean)),
  textlink: (( Text: string ) -> (boolean)),
  stylecolors: (( Theme: 'L' | 'D' ) -> ()),
  setnextwindowposition: (( X: number, Y: number ) -> ()),
  setnextwindowfocus: ((  ) -> ()),
  beginchild: (( Name: string, Width: number?, Height: number?, ChildFlags: string?, WindowFlags: string? ) -> (boolean)),
  popid: ((  ) -> ()),
  setitemtooltip: (( Text: string ) -> ()),
  setcursorpos: (( X: number, Y: number ) -> ()),
  separatortext: (( Text: string ) -> ()),
  beginmenu: (( Name: string, Enabled: boolean? ) -> (boolean)),
  separator: ((  ) -> ()),
  pushstylecolor: (( StyleIndex: number, Color: { number } ) -> ()),
  pushid: (( Id: string ) -> ()),
  endchild: ((  ) -> ()),
  getcontentregionavail: ((  ) -> (number, number)),
  anyitemactive: ((  ) -> (boolean)),
  itemclicked: ((  ) -> (boolean)),
  indent: (( Indent: number? ) -> ()),
  itemhovered: ((  ) -> (boolean)),
  popstylecolor: ((  ) -> ()),
  getwindowposition: ((  ) -> (number, number)),
  beginfullscreen: (( Name: string, OffsetX: number?, OffsetY: number? ) -> ()),
  endmainmenubar: ((  ) -> ()),
  beginmenubar: ((  ) -> (boolean)),
  imagebutton: (( Name: string, Image: string, Size: { number }? ) -> (boolean))
}
declare input: {
  ismousegrabbed: ((  ) -> (boolean)),
  setmousegrabbed: (( Grabbed: boolean ) -> ()),
  guihandledm: ((  ) -> (boolean)),
  setmousepos: (( X: number, Y: number ) -> ()),
  iscursorvisible: ((  ) -> (boolean)),
  setcursorvisible: (( Visible: boolean ) -> ()),
  getmousepos: ((  ) -> (number, number)),
  guihandled: ((  ) -> (boolean)),
  mousedown: (( Button: 'l' | 'r' | 'e' ) -> (boolean)),
  keypressed: (( Key: string ) -> (boolean)),
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
declare net: {
  request: ((  ) -> ())
}
declare scene: {
  load: (( Path: string ) -> ({ GameObject }?, string?)),
  save: (( RootNodes: { GameObject }, Path: string ) -> (boolean))
}
declare world: {
  aabbcast: (( APos: vector, ASize: vector, IgnoreList: { GameObject } ) -> ({ Object: GameObject, Position: vector, Normal: vector }?)),
  raycast: (( Origin: vector, CastVector: vector, IgnoreList: { GameObject } ) -> ({ Object: GameObject, Position: vector, Normal: vector }?)),
  aabbquery: (( APos: vector, ASize: vector, IgnoreList: { GameObject } ) -> ({ GameObject }))
}


declare _VMNAME: string
declare function appendlog( ...: any ): ()
declare function breakpoint( Line: number ): ()
declare game: GameObject & DataModel
declare function loadthread( Code: string, ChunkName: string? ): ( thread?, string? )
declare function loadthreadfromfile( File: string, ChunkName: string? ): ( thread?, string? )
declare script: GameObject & Script
declare function sleep( SleepTime: number ): ()
declare workspace: GameObject & Workspace



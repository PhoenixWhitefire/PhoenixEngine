declare class Vector2
end
declare class Color
  R: number
  G: number
  B: number
end
declare Color: {
  new: (( R: number, G: number, B: number ) -> (Color))
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
  new: ((    ) -> (Matrix)),
  fromTranslation: (( Position: vector ) -> (Matrix)) & (( X: number, Y: number, Z: number ) -> (Matrix)),
  lookAt: (( Eye: vector, Target: vector ) -> (Matrix)),
  fromEulerAnglesXYZ: (( X: number, Y: number, Z: number ) -> (Matrix))
}


declare class GameObject
  Enabled: boolean
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
  save: ((    ) -> (boolean)),
  set: (( Key: string, Value: any ) -> ()),
  get: (( Key: string ) -> (any))
}
declare engine: {
  framerate: (( any ) -> (any)),
  setvsync: (( any ) -> (any)),
  setwindowsize: (( any ) -> (any)),
  exit: (( ExitCode: number? ) -> ()),
  setfullscreen: (( any ) -> (any)),
  setmaxframerate: (( MaxFramerate: number ) -> ()),
  getfullscreen: (( any ) -> (any)),
  getvsync: (( any ) -> (any)),
  getmaxframerate: (( any ) -> (any)),
  getwindowsize: (( any ) -> (any)),
  dwireframes: (( Enabled: boolean? ) -> ()),
  isheadless: (( any ) -> (any)),
  daabbs: (( Enabled: boolean? ) -> ())
}
declare fs: {
  write: (( Path: string, Contents: string ) -> (boolean)),
  read: (( Path: string ) -> (string?)),
  promptopen: (( DefaultLocation: string?, Filter: string?, FilterName: string?, AllowMultipleFiles: boolean ) -> ({ string })),
  isfile: (( any ) -> (any)),
  promptsave: (( DefaultLocation: string?, Filter: string?, FilterName: string? ) -> (string)),
  listdir: (( Path: string, Filter: 'a' | 'f' | 'd' ) -> ({ [string]: 'f' | 'd' })),
  isdirectory: (( any ) -> (any)),
  ispromptactive: (( any ) -> (any))
}
declare imgui: {
  endw: (( any ) -> (any)),
  textlink: (( any ) -> (any)),
  itemclicked: (( any ) -> (any)),
  image: (( any ) -> (any)),
  button: (( any ) -> (any)),
  inputstring: (( any ) -> (any)),
  checkbox: (( any ) -> (any)),
  setitemtooltip: (( any ) -> (any)),
  text: (( any ) -> (any)),
  itemhovered: (( any ) -> (any)),
  begin: (( any ) -> (any)),
  inputnumber: (( any ) -> (any)),
  indent: (( any ) -> (any))
}
declare input: {
  ismousegrabbed: ((    ) -> (boolean)),
  setmousegrabbed: (( Grabbed: boolean ) -> ()),
  guihandledm: ((    ) -> (boolean)),
  setmousepos: (( X: number, Y: number ) -> ()),
  iscursorvisible: ((    ) -> (boolean)),
  setcursorvisible: (( Visible: boolean ) -> ()),
  getmousepos: ((    ) -> (number, number)),
  guihandled: ((    ) -> (boolean)),
  mousedown: (( Button: 'l' | 'r' | 'b' ) -> ()),
  keypressed: (( Key: string ) -> (boolean)),
  guihandledk: ((    ) -> (boolean))
}
declare json: {
  encode: (( any ) -> (any)),
  parse: (( any ) -> (any))
}
declare mesh: {
  save: (( any ) -> (any)),
  set: (( any ) -> (any)),
  get: (( any ) -> (any))
}
declare model: {
  import: (( any ) -> (any))
}
declare net: {
  request: (( any ) -> (any))
}
declare scene: {
  load: (( any ) -> (any)),
  save: (( any ) -> (any))
}
declare world: {
  aabbcast: (( any ) -> (any)),
  raycast: (( any ) -> (any)),
  aabbquery: (( any ) -> (any))
}


declare function appendlog( ...: any ): ()
declare function breakpoint( Line: number ): ()
declare game: GameObject & DataModel
declare function loadfile( File: string, ChunkName: string ): ( thread?, string? )
declare function loadthread( Code: string, ChunkName: string ): ( thread?, string? )
declare function sleep( SleepTime: number ): ()
declare workspace: GameObject & Workspace



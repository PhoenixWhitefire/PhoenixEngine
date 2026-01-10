-- mc.luau, 18/09/2024

local Cubes = {}
local NumCubes = 0

local TotalTime = 0

local Camera = workspace.SceneCamera

function Update(DeltaTime)
    TotalTime += DeltaTime
end

local seed = math.random(1, 1000)

local chunkMesh = mesh_get("!Cube")

local function addCube(Position)
	local chv = chunkMesh.Vertices
	local chi = chunkMesh.Indices
	
	local cbm = mesh_get("!Cube")
	
	for i, v in cbm.Vertices do
		cbm.Vertices[i].Position += Position
	end
	
	for i, v in cbm.Indices do
		cbm.Indices[i] += #chi
	end
	
	table.move(cbm.Vertices, 1, #cbm.Vertices, #chunkMesh.Vertices, chunkMesh.Vertices)
	table.move(cbm.Indices, 1, #cbm.Indices, #chunkMesh.Indices, chunkMesh.Indices)
end

for x = -10, 10 do
    for z = -10, 10 do
    	addCube(vector.create(x, math.noise(x + .1, z, seed) * 5, z))
    end
end

mesh_set("$mcchunk_0-0", chunkMesh)

local chunk = GameObject.new("Mesh")
chunk.Name = "mcchunk_0-0"
chunk.Asset = "$mcchunk_0-0"
chunk.Size = vector.create(10, 5, 10)
chunk.Parent = workspace.Level


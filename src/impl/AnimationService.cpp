#include <AnimationService.hpp>

AnimationService* AnimationService::Inst = nullptr;

/*
Vector3 GetVector3FromJSON(nlohmann::json JSON) {
	int Index = -1;

	Vector3 Vec3;

	try {
		Vec3 = Vector3(
			JSON[++Index],
			JSON[++Index],
			JSON[++Index]
		);
	}
	catch (const nlohmann::json::type_error& TErr) {
		Log.Info("Could not read Vector3!\nError: '" + std::string(TErr.what()) + "'");
	}

	return Vec3;
}
*/

/*
pose_t::pose_t(Vector3 position, Vector3 orientation) {
	this->Position = position;
	this->Orientation = orientation;
}

keyframe_t& animation_t::GetKeyframeAtTime(float Time) {
	return this->GetKeyframeAtPercent(Time / this->TimeLength);
}

keyframe_t& animation_t::GetKeyframeAtPercent(float Alpha) {
	return this->KeyframeRange.GetValue(Alpha);
}
AnimationService& AnimationService::Get() {
	if (!AnimationService::Inst) {
		AnimationService::Inst = new AnimationService();
	}

	return *AnimationService::Inst;
}
*/
animation_t& AnimationService::LoadAnimationJSON(const char* File) {
	throw(std::runtime_error("Animator class is disabled @ " + std::string(File)));

	/*
	std::string FileContentsString = FileRW::ReadFile(File);

	nlohmann::json JSONData = nlohmann::json::parse(FileContentsString);

	animation_t LoadedAnimation = animation_t();

	LoadedAnimation.TimeLength = std::stof((std::string)JSONData["animlength"]);

	nlohmann::json AnimationKeyframes = JSONData["animkeys"];

	for (int KeyframeIndex = 0; KeyframeIndex < AnimationKeyframes.size(); KeyframeIndex++) {
		nlohmann::json KeyframeData = AnimationKeyframes[KeyframeIndex];

		keyframe_t Keyframe;

		Keyframe.TimePosition = KeyframeData["kftime"];
		Keyframe.AlphaTime = Keyframe.TimePosition / LoadedAnimation.TimeLength;

		nlohmann::json KeyframePoses = KeyframeData["posedata"];

		for (int PoseIndex = 0; PoseIndex < KeyframePoses.size(); PoseIndex++) {
			nlohmann::json PoseData = KeyframePoses[PoseIndex];

			pose_t Pose;

			Pose.Position = GetVector3FromJSON(PoseData["bpos"]);
			Pose.Orientation = GetVector3FromJSON(PoseData["brot"]);

			Pose.BoneName = PoseData["bname"];

			Keyframe.Poses.push_back(Pose);
		}

		LoadedAnimation.KeyframeRange.InsertKey(ValueRangeKey<keyframe_t&>(Keyframe, Keyframe.AlphaTime));
	}

	return LoadedAnimation;
	*/
}

//AnimationService needs the Instance header file, but the Instance header file #includes it as well. how fix? 30/05/2022 phoenix
/*
void AnimationService::UpdateBones(animation_t& Animation, std::shared_ptr<Instance> RootInstance) {
	keyframe_t CurrentKeyframe = Animation.GetKeyframeAtTime(Animation.CurrentTime);

	std::vector<pose_t> PoseData = CurrentKeyframe.Poses;

	for (int PoseIndex = 0; PoseIndex < PoseData.size(); PoseIndex++) {
		pose_t CurrentPose = PoseData[PoseIndex];

		std::shared_ptr<Instance> b_inst = RootInstance->GetChild(CurrentPose.BoneName);

		if (b_inst != nullptr) {
			std::shared_ptr<MeshPart> Bone = std::dynamic_pointer_cast<MeshPart>(b_inst);

			Bone->Position = CurrentPose.Position;
			Bone->Orientation = CurrentPose.Orientation;
		}
	}
}
*/
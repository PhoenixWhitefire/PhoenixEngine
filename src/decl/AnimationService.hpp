#pragma once

#include<iostream>

/*
#include<nljson/json.h>
#include<vector>

#include"ValueRange.hpp"
#include"Vector3.hpp"
#include"FileRW.hpp"
*/

/*
class pose_t {
	public:
		pose_t(Vector3 position = Vector3(), Vector3 orientation = Vector3());

		Vector3 Position;
		Vector3 Orientation;

		std::string BoneName;

		pose_t operator - (pose_t Other) {
			return pose_t(
				this->Position - Other.Position,
				this->Orientation - Other.Orientation
			);
		}

		pose_t operator * (pose_t Other) {
			return pose_t(
				this->Position * Other.Position,
				this->Orientation * Other.Orientation
			);
		}

		pose_t operator + (pose_t Other) {
			return pose_t(
				this->Position + Other.Position,
				this->Orientation + Other.Orientation
			);
		}
};

class keyframe_t {
	public:
		std::vector<pose_t> Poses;

		float TimePosition;
		float AlphaTime;

		keyframe_t operator - (keyframe_t Other) {
			keyframe_t NewKeyframe;

			for (int PoseIndex = 0; PoseIndex < this->Poses.size(); PoseIndex++) {
				if (PoseIndex >= Other.Poses.size()) {
					NewKeyframe.Poses.push_back(pose_t());

					continue;
				}

				NewKeyframe.Poses.push_back(this->Poses[PoseIndex] - Other.Poses[PoseIndex]);
			}

			return NewKeyframe;
		}

		keyframe_t operator * (keyframe_t Other) {
			keyframe_t NewKeyframe;

			for (int PoseIndex = 0; PoseIndex < this->Poses.size(); PoseIndex++) {
				if (PoseIndex >= Other.Poses.size()) {
					NewKeyframe.Poses.push_back(pose_t());

					continue;
				}

				NewKeyframe.Poses.push_back(this->Poses[PoseIndex] * Other.Poses[PoseIndex]);
			}

			return NewKeyframe;
		}

		keyframe_t operator - (keyframe_t Other) {
			keyframe_t NewKeyframe;

			for (int PoseIndex = 0; PoseIndex < this->Poses.size(); PoseIndex++) {
				if (PoseIndex >= Other.Poses.size()) {
					NewKeyframe.Poses.push_back(pose_t());

					continue;
				}

				NewKeyframe.Poses.push_back(this->Poses[PoseIndex] + Other.Poses[PoseIndex]);
			}

			return NewKeyframe;
		}
};
*/

class animation_t {
	public:
		float TimeLength;
		float CurrentTime; //Needs to be manually updated in engine loop

		//keyframe_t& GetKeyframeAtTime(float Time);
		//keyframe_t& GetKeyframeAtPercent(float Alpha);

		//ValueRange<keyframe_t&> KeyframeRange;
		
};


class AnimationService {
	public:
		static AnimationService& Get();

		animation_t& LoadAnimationJSON(const char* File);
		//void UpdateBones(animation_t& Animation, std::shared_ptr<Instance> RootInstance);

	private:
		static AnimationService* Inst;
};

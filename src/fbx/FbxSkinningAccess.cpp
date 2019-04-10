/**
 * Copyright (c) 2014-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "FbxSkinningAccess.hpp"

FbxSkinningAccess::FbxSkinningAccess(const FbxMesh* pMesh, FbxScene* pScene, FbxNode* pNode)
    : rootIndex(-1) {
  for (int deformerIndex = 0; deformerIndex < pMesh->GetDeformerCount(); deformerIndex++) {
    FbxSkin* skin =
        reinterpret_cast<FbxSkin*>(pMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
    if (skin != nullptr) {
      const int clusterCount = skin->GetClusterCount();
      if (clusterCount == 0) {
        continue;
      }
      int controlPointCount = pMesh->GetControlPointsCount();
      vertexJointIndices.resize(controlPointCount);
      vertexJointWeights.resize(controlPointCount);

      for (int clusterIndex = 0; clusterIndex < clusterCount; clusterIndex++) {
        FbxCluster* cluster = skin->GetCluster(clusterIndex);
        const int indexCount = cluster->GetControlPointIndicesCount();
        const int* clusterIndices = cluster->GetControlPointIndices();
        const double* clusterWeights = cluster->GetControlPointWeights();

        assert(cluster->GetLinkMode() == FbxCluster::eNormalize);

        // Transform link matrix.
        FbxAMatrix transformLinkMatrix;
        cluster->GetTransformLinkMatrix(transformLinkMatrix);

        // The transformation of the mesh at binding time
        FbxAMatrix transformMatrix;
        cluster->GetTransformMatrix(transformMatrix);

        // Inverse bind matrix.
        FbxAMatrix globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix;
        inverseBindMatrices.emplace_back(globalBindposeInverseMatrix);

        jointNodes.push_back(cluster->GetLink());
        jointIds.push_back(cluster->GetLink()->GetUniqueID());

        const FbxAMatrix globalNodeTransform = cluster->GetLink()->EvaluateGlobalTransform();
        jointSkinningTransforms.push_back(
            FbxMatrix(globalNodeTransform * globalBindposeInverseMatrix));
        jointInverseGlobalTransforms.push_back(FbxMatrix(globalNodeTransform.Inverse()));

        for (int i = 0; i < indexCount; i++) {
          if (clusterIndices[i] < 0 || clusterIndices[i] >= controlPointCount) {
            continue;
          }
          if (clusterWeights[i] <= 0.0 || vertexJointWeights[clusterIndices[i]].size() >= MAX_WEIGHTS) {
            continue;
          }
          vertexJointIndices[clusterIndices[i]].push_back(clusterIndex);
          vertexJointWeights[clusterIndices[i]].push_back((float)clusterWeights[i]);
          for (int j = vertexJointWeights[clusterIndices[i]].size() - 1; j > 0; j--) {
            if (vertexJointWeights[clusterIndices[i]][j - 1] >=
                vertexJointWeights[clusterIndices[i]][j]) {
              break;
            }
            std::swap(
                vertexJointIndices[clusterIndices[i]][j - 1],
                vertexJointIndices[clusterIndices[i]][j]);
            std::swap(
                vertexJointWeights[clusterIndices[i]][j - 1],
                vertexJointWeights[clusterIndices[i]][j]);
          }
        }
      }
      for (int i = 0; i < controlPointCount; i++) {
        float weightSum = 0.0;
        for (int w = 0; w < vertexJointWeights[i].size(); ++w)
          weightSum += vertexJointWeights[i][w];
        float weightSumRcp = 1.0 / weightSum;
        for (int w = 0; w < vertexJointWeights[i].size(); ++w)
          vertexJointWeights[i][w] *= weightSumRcp;
      }
    }
  }

  rootIndex = -1;
  for (size_t i = 0; i < jointNodes.size() && rootIndex == -1; i++) {
    rootIndex = (int)i;
    FbxNode* parent = jointNodes[i]->GetParent();
    if (parent == nullptr) {
      break;
    }
    for (size_t j = 0; j < jointNodes.size(); j++) {
      if (jointNodes[j] == parent) {
        rootIndex = -1;
        break;
      }
    }
  }
}

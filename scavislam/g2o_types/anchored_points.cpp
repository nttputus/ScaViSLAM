// This file is part of ScaViSLAM.
//
// Copyright 2011 Hauke Strasdat (Imperial College London)
//
// ScaViSLAM is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// ScaViSLAM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with ScaViSLAM.  If not, see <http://www.gnu.org/licenses/>.

#include "anchored_points.h"

#include "../transformations.h"

namespace ScaViSLAM
{

G2oVertexSE3
::G2oVertexSE3()
  : principle_point(Vector2d(0., 0.)),
    focal_length(1.),
    baseline(0.5)
{
}

void G2oVertexSE3
::oplus(double * update_p)
{
  Map<Vector6d> update(update_p);
  estimate() = SE3::exp(update)*estimate();
}

Vector2d  G2oVertexSE3
::cam_map(const Vector3d & trans_xyz) const
{
  Vector2d proj = project2d(trans_xyz);
  Vector2d res;
  res[0] = proj[0]*focal_length + principle_point[0];
  res[1] = proj[1]*focal_length + principle_point[1];
  return res;
}

Vector3d G2oVertexSE3
::stereocam_uvu_map(const Vector3d & trans_xyz) const
{
  Vector2d uv_left = cam_map(trans_xyz);
  double proj_x_right = (trans_xyz[0]-baseline)/trans_xyz[2];
  double u_right = proj_x_right*focal_length + principle_point[0];
  return Vector3d(uv_left[0],uv_left[1],u_right);
}

//TODO: implement, but first remove camera parameters from vertex state
bool G2oVertexSE3
::write(std::ostream& os) const
{
  assert(false);
  return true;
}

//TODO: implement, but first remove camera parameters from vertex state
bool G2oVertexSE3
::read(std::istream& is)
{
  assert(false);
  return true;
}




void G2oVertexPointXYZ
::oplus(double * update_p)
{
  Map<Vector3d> update(update_p);
  _estimate += update;
}

bool G2oVertexPointXYZ
::write (std::ostream & os) const
{
  const Vector3d & lv = estimate();
  for (int i=0; i<3; i++)
  {
    os << lv[i] << " ";
  }
  return true;
}

bool G2oVertexPointXYZ
::read(std::istream& is)
{
  Vector3d lv;
  for (int i=0; i<3; i++)
  {
    is >> lv[i];
  }
  estimate() = lv;
  return true;
}


bool G2oEdgeProjectPSI2UVU
::write(std::ostream& os) const
{
  for (int i=0; i<3; i++)
  {
    os  << measurement()[i] << " ";
    {
      for (int i=0; i<3; i++)
        for (int j=i; j<3; j++)
        {
          os << " " <<  information()(i,j);
        }
    }
  }
      return true;
}

bool G2oEdgeProjectPSI2UVU
::read(std::istream& is)
{
  for (int i=0; i<3; i++)
  {
    is  >> measurement()[i];
  }
  inverseMeasurement()[0] = -measurement()[0];
  inverseMeasurement()[1] = -measurement()[1];
  inverseMeasurement()[2] = -measurement()[2];

  for (int i=0; i<3; i++)
    for (int j=i; j<3; j++)
    {
      is >> information()(i,j);
      if (i!=j)
        information()(j,i)=information()(i,j);
    }
  return true;
}

void G2oEdgeProjectPSI2UVU
::computeError()
{
  const G2oVertexPointXYZ * psi
      = static_cast<const G2oVertexPointXYZ*>(_vertices[0]);
  const G2oVertexSE3 * T_p_from_world
      = static_cast<const G2oVertexSE3*>(_vertices[1]);
  const G2oVertexSE3 * T_anchor_from_world
      = static_cast<const G2oVertexSE3*>(_vertices[2]);

  Vector3d obs(_measurement);
  _error = obs - T_p_from_world->stereocam_uvu_map(
             T_p_from_world->estimate()
             *T_anchor_from_world->estimate().inverse()
             *invert_depth(psi->estimate()));
}

void G2oEdgeProjectPSI2UVU::linearizeOplus()
{
  G2oVertexPointXYZ* vpoint = static_cast<G2oVertexPointXYZ*>(_vertices[0]);
  Vector3d psi_a = vpoint->estimate();
  G2oVertexSE3 * vpose = static_cast<G2oVertexSE3 *>(_vertices[1]);
  SE3 T_cw = vpose->estimate();
  G2oVertexSE3 * vanchor = static_cast<G2oVertexSE3 *>(_vertices[2]);
  SE3 A_aw = vanchor->estimate();
  SE3 T_ca = T_cw*A_aw.inverse();
  Vector3d x_a = invert_depth(psi_a);
  Vector3d y = T_ca*x_a;
  Matrix3d Jcam
      = d_stereoproj_d_y(vpose->focal_length,
                         vpose->baseline,
                         y);
  _jacobianOplus[0] = -Jcam*d_Tinvpsi_d_psi(T_ca, psi_a);
  _jacobianOplus[1] = -Jcam*d_expy_d_y(y);
  _jacobianOplus[2] = Jcam*T_ca.rotation_matrix()*d_expy_d_y(x_a);
}



bool G2oEdgeSE3
::read(std::istream& is)
{
  assert(false);
  return true;
}

bool G2oEdgeSE3
::write(std::ostream& os) const
{
  assert(false);
  return true;
}

void G2oEdgeSE3
::computeError()
{
  const G2oVertexSE3 * v1 = static_cast<const G2oVertexSE3 *>(_vertices[0]);
  const G2oVertexSE3 * v2 = static_cast<const G2oVertexSE3 *>(_vertices[1]);
  SE3 C(_measurement);
  SE3 error_= v2->estimate().inverse()*C*v1->estimate();
  _error = error_.log();
}

void G2oEdgeSE3::
linearizeOplus()
{
  G2oVertexSE3 * vi = static_cast<G2oVertexSE3 *>(_vertices[0]);
  SE3 Ti(vi->estimate());
  G2oVertexSE3 * vj = static_cast<G2oVertexSE3 *>(_vertices[1]);
  SE3 Tj(vj->estimate());
  const SE3 & Tij = _measurement;
  SE3 invTij = Tij.inverse();
  SE3 invTj_Tij = Tj.inverse()*Tij;
  SE3 infTi_invTij = Ti.inverse()*invTij;

  //TODO: this is only a rough first order approximation!!
  _jacobianOplusXi = invTj_Tij.Adj();
  _jacobianOplusXj = -infTi_invTij.Adj();
}

#ifdef MONO

G2oVertexSim3
::G2oVertexSim3()
  : principle_point(Vector2d(0., 0.)),
    focal_length(1.)
{
}

void G2oVertexSim3
::oplus(double * update_p)
{
  Map<Vector7d> update(update_p);
  estimate() = Sim3::exp(update)*estimate();
}

Vector2d  G2oVertexSim3
::cam_map(const Vector2d & v) const
{
  Vector2d res;
  res[0] = v[0]*focal_length + principle_point[0];
  res[1] = v[1]*focal_length + principle_point[1];
  return res;
}

//TODO: implement, but first remove camera parameters from vertex state
bool G2oVertexSim3
::write(std::ostream& os) const
{
  assert(false);
  return true;
}

//TODO: implement, but first remove camera parameters from vertex state
bool G2oVertexSim3
::read(std::istream& is)
{
  assert(false);
  return true;
}


void G2oEdgeSim3ProjectUVQ::
computeError()
{
  const G2oVertexPointXYZ* psi
      = static_cast<const G2oVertexPointXYZ*>(_vertices[0]);
  const G2oVertexSE3* T_p_from_world
      = static_cast<const G2oVertexSE3*>(_vertices[1]);
  const G2oVertexSE3* T_anchor_from_world
      = static_cast<const G2oVertexSE3*>(_vertices[2]);

  Vector2d obs(_measurement);
  _error = obs-T_p_from_world->cam_map(
             T_p_from_world->estimate()
             *T_anchor_from_world->estimate().inverse()
             *invert_depth(psi->estimate()));
}

bool G2oEdgeSim3ProjectUVQ
::read(std::istream & is)
{
  for (int i=0; i<2; i++)
  {
    is >> measurement()[i];
  }
  inverseMeasurement()[0] = -measurement()[0];
  inverseMeasurement()[1] = -measurement()[1];
  for (int i=0; i<2; i++)
    for (int j=i; j<2; j++)
    {
      is >> information()(i,j);
      if (i!=j)
        information()(j,i) = information()(i,j);
    }
  return true;
}

bool  G2oEdgeSim3ProjectUVQ
::write(std::ostream& os) const
{
  for (int i=0; i<2; i++)
  {
    os  << measurement()[i] << " ";
  }
  for (int i=0; i<2; i++)
    for (int j=i; j<2; j++)
    {
      os << " " <<  information()(i,j);
    }
  return true;
}

bool G2oEdgeSim3
::read(std::istream& is)
{
  Vector7d v7;
  for (int i=0; i<7; i++)
  {
    is >> v7[i];
  }
  Sim3 cam2world  = Sim3::exp(v7);
  measurement() = cam2world.inverse();
  inverseMeasurement() = cam2world;
  for (int i=0; i<7; i++)
    for (int j=i; j<7; j++)
    {
      is >> information()(i,j);
      if (i!=j)
        information()(j,i) = information()(i,j);
    }
  return is;
}

bool G2oEdgeSim3
::write(std::ostream& os) const
{
  Sim3 cam2world(measurement().inverse());
  Vector7d v7 = cam2world.log();
  for (int i=0; i<7; i++)
  {
    os  << v7[i] << " ";
  }
  for (int i=0; i<7; i++)
    for (int j=i; j<7; j++){
      os << " " <<  information()(i,j);
    }
  return true;
}


void G2oEdgeSim3
::computeError()
{
  const G2oVertexSim3* v1 = static_cast<const G2oVertexSim3*>(_vertices[0]);
  const G2oVertexSim3* v2 = static_cast<const G2oVertexSim3*>(_vertices[1]);
  Sim3 C(_measurement);
  Sim3 error_= v2->estimate().inverse()*C*v1->estimate();
  _error = error_.log().head<6>();
}
#endif

}





















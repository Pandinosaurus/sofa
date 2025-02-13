/******************************************************************************
*                 SOFA, Simulation Open-Framework Architecture                *
*                    (c) 2006 INRIA, USTL, UJF, CNRS, MGH                     *
*                                                                             *
* This program is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This program is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this program. If not, see <http://www.gnu.org/licenses/>.        *
*******************************************************************************
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/
#define SOFA_COMPONENT_CONSTRAINT_GENERICCONSTRAINTCORRECTION_CPP

#include <sofa/component/constraint/lagrangian/correction/GenericConstraintCorrection.h>
#include <sofa/simulation/mechanicalvisitor/MechanicalIntegrateConstraintVisitor.h>
#include <sofa/core/behavior/OdeSolver.h>
#include <sofa/core/behavior/LinearSolver.h>
#include <sofa/core/ObjectFactory.h>
#include <sofa/core/behavior/ConstraintSolver.h>
#include <sofa/core/ConstraintParams.h>

using sofa::core::execparams::defaultInstance; 

namespace sofa::component::constraint::lagrangian::correction 
{
using sofa::simulation::mechanicalvisitor::MechanicalIntegrateConstraintsVisitor;
using sofa::type::vector;
using sofa::core::objectmodel::BaseContext;
using sofa::core::behavior::LinearSolver;
using sofa::core::behavior::BaseConstraintCorrection;
using sofa::core::behavior::ConstraintSolver;
using sofa::linearalgebra::BaseMatrix;
using sofa::core::ConstraintParams;
using sofa::core::MultiVecDerivId;
using sofa::core::MultiVecCoordId;
using sofa::core::ExecParams;
using sofa::linearalgebra::BaseVector;
using sofa::core::RegisterObject;
using sofa::core::ConstMultiVecDerivId;
using sofa::core::VecDerivId;
using sofa::core::VecCoordId;

GenericConstraintCorrection::GenericConstraintCorrection()
: d_linearSolversName( initData(&d_linearSolversName, "solverName", "name of the constraint solver") )
, d_ODESolverName( initData(&d_ODESolverName, "ODESolverName", "name of the ode solver") )
, d_complianceFactor(initData(&d_complianceFactor, 1.0, "complianceFactor", "Factor applied to the position factor and velocity factor used to calculate compliance matrix"))
{
    m_ODESolver = nullptr;
}

GenericConstraintCorrection::~GenericConstraintCorrection() {}

void GenericConstraintCorrection::bwdInit()
{
    BaseContext* context = this->getContext();

    // Find linear solvers
    m_linearSolvers.clear();
    const vector<std::string>& solverNames = d_linearSolversName.getValue();
    if(solverNames.empty())
    {
        LinearSolver* s = nullptr;
        context->get(s);
        if(s)
        {
            if (s->getTemplateName() == "GraphScattered")
                msg_warning() << "Can not use the solver " << s->getName() << " because it is templated on GraphScatteredType";
            else
                m_linearSolvers.push_back(s);
        }
    }
    else 
    {
        for(unsigned int i=0; i<solverNames.size(); ++i)
        {
            LinearSolver* s = nullptr;
            context->get(s, solverNames[i]);

            if(s)
            {
                if (s->getTemplateName() == "GraphScattered")
                    msg_warning() << "Can not use the solver " << solverNames[i] << " because it is templated on GraphScatteredType";
                else
                    m_linearSolvers.push_back(s);
            } 
            else
                msg_warning() << "Solver \"" << solverNames[i] << "\" not found.";
        }

    }

    // Find ODE solver
    if(d_ODESolverName.isSet())
        context->get(m_ODESolver, d_ODESolverName.getValue());

    if(m_ODESolver == nullptr)
    {
        context->get(m_ODESolver, BaseContext::Local);
        if (m_ODESolver == nullptr)
        {
            context->get(m_ODESolver, BaseContext::SearchRoot);
            if (m_ODESolver == nullptr)
            {
                msg_error() << "No OdeSolver found.";
                return;
            }
        }
    }

    if (m_linearSolvers.empty())
    {
        msg_error() << "No LinearSolver found.";
        return;
    }

    msg_info() << "Found " << m_linearSolvers.size() << " m_linearSolvers";
    for (auto* linearSolver : m_linearSolvers)
        msg_info() << linearSolver->getName();
}

void GenericConstraintCorrection::cleanup()
{
    while(!constraintsolvers.empty())
    {
        constraintsolvers.back()->removeConstraintCorrection(this);
        constraintsolvers.pop_back();
    }
    BaseConstraintCorrection::cleanup();
}

void GenericConstraintCorrection::addConstraintSolver(ConstraintSolver *s)
{
    constraintsolvers.push_back(s);
}

void GenericConstraintCorrection::removeConstraintSolver(ConstraintSolver *s)
{
    constraintsolvers.remove(s);
}

void GenericConstraintCorrection::rebuildSystem(double massFactor, double forceFactor)
{
    for (auto* linearSolver : m_linearSolvers)
        linearSolver->rebuildSystem(massFactor, forceFactor);
}

void GenericConstraintCorrection::addComplianceInConstraintSpace(const ConstraintParams *cparams, BaseMatrix* W)
{
    if (!m_ODESolver) return;
    const double complianceFactor = d_complianceFactor.getValue();

    // use the OdeSolver to get the position integration factor
    double factor = 1.0;

    switch (cparams->constOrder())
    {
        case ConstraintParams::POS_AND_VEL :
        case ConstraintParams::POS :
            factor = m_ODESolver->getPositionIntegrationFactor();
            break;

        case ConstraintParams::ACC :
        case ConstraintParams::VEL :
            factor = m_ODESolver->getVelocityIntegrationFactor();
            break;

        default :
            break;
    }

    factor *= complianceFactor;
    // use the Linear solver to compute J*inv(M)*Jt, where M is the mechanical linear system matrix
    for (auto* linearSolver : m_linearSolvers)
    {
        linearSolver->buildComplianceMatrix(cparams, W, factor);
    }
}

void GenericConstraintCorrection::computeMotionCorrectionFromLambda(const ConstraintParams* cparams, MultiVecDerivId dx, const linearalgebra::BaseVector * lambda)
{
    for (auto* linearSolver : m_linearSolvers)
    {
        linearSolver->applyConstraintForce(cparams, dx, lambda);
    }
}

void GenericConstraintCorrection::applyMotionCorrection(const ConstraintParams* cparams,
                                                        MultiVecCoordId xId,
                                                        MultiVecDerivId vId, 
                                                        MultiVecDerivId dxId,
                                                        ConstMultiVecDerivId correction,
                                                        double positionFactor,
                                                        double velocityFactor)
{
    for (auto* linearSolver : m_linearSolvers)
    {
        MechanicalIntegrateConstraintsVisitor v(cparams, positionFactor, velocityFactor, correction, dxId, xId, vId, linearSolver->getSystemMultiMatrixAccessor());
        linearSolver->getContext()->executeVisitor(&v);
    }
}

void GenericConstraintCorrection::applyMotionCorrection(const ConstraintParams * cparams,
                                                        MultiVecCoordId xId,
                                                        MultiVecDerivId vId,
                                                        MultiVecDerivId dxId,
                                                        ConstMultiVecDerivId correction)
{
    if (!m_ODESolver) return;
    const double complianceFactor = d_complianceFactor.getValue();

    const double positionFactor = m_ODESolver->getPositionIntegrationFactor() * complianceFactor;
    const double velocityFactor = m_ODESolver->getVelocityIntegrationFactor() * complianceFactor;

    applyMotionCorrection(cparams, xId, vId, dxId, correction, positionFactor, velocityFactor);
}

void GenericConstraintCorrection::applyPositionCorrection(const ConstraintParams * cparams,
                                                          MultiVecCoordId xId,
                                                          MultiVecDerivId dxId,
                                                          ConstMultiVecDerivId correctionId)
{
    if (!m_ODESolver) return;

    const double complianceFactor = d_complianceFactor.getValue();
    const double positionFactor = m_ODESolver->getPositionIntegrationFactor() * complianceFactor;

    applyMotionCorrection(cparams, xId, VecDerivId::null(), dxId, correctionId, positionFactor, 0);
}

void GenericConstraintCorrection::applyVelocityCorrection(const ConstraintParams * cparams,
                                                          MultiVecDerivId vId,
                                                          MultiVecDerivId dvId,
                                                          ConstMultiVecDerivId correctionId)
{
    if (!m_ODESolver) return;

    const double velocityFactor = m_ODESolver->getVelocityIntegrationFactor();

    applyMotionCorrection(cparams, VecCoordId::null(), vId, dvId, correctionId, 0, velocityFactor);
}

void GenericConstraintCorrection::applyContactForce(const BaseVector *f)
{
    if (!m_ODESolver) return;

    ConstraintParams cparams(*sofa::core::execparams::defaultInstance());


    computeMotionCorrectionFromLambda(&cparams, cparams.dx(), f);
    applyMotionCorrection(&cparams, VecCoordId::position(), VecDerivId::velocity(), cparams.dx(), cparams.lambda());
}

void GenericConstraintCorrection::computeResidual(const ExecParams* params, linearalgebra::BaseVector *lambda)
{
    for (auto* linearSolver : m_linearSolvers)
    {
        linearSolver->computeResidual(params, lambda);
    }
}


void GenericConstraintCorrection::getComplianceMatrix(linearalgebra::BaseMatrix* Minv) const
{
    if (!m_ODESolver)
        return;

    ConstraintParams cparams(*sofa::core::execparams::defaultInstance());
    const_cast<GenericConstraintCorrection*>(this)->addComplianceInConstraintSpace(&cparams, Minv);
}

void GenericConstraintCorrection::applyPredictiveConstraintForce(const ConstraintParams * cparams,
                                                                 MultiVecDerivId f,
                                                                 const BaseVector * lambda)
{
    SOFA_UNUSED(cparams);
    SOFA_UNUSED(f);
    SOFA_UNUSED(lambda);
}

void GenericConstraintCorrection::resetContactForce(){}

int GenericConstraintCorrectionClass = RegisterObject("")
    .add< GenericConstraintCorrection >();

} //namespace sofa::component::constraint::lagrangian::correction

/* Copyright 2013-2017 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with Homegear.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#include "RpcMethod.h"
#include "../BaseLib.h"

namespace BaseLib
{
namespace Rpc
{
PVariable RpcMethod::invoke(PRpcClientInfo clientInfo, std::shared_ptr<std::vector<PVariable>> parameters)
{
	return PVariable(new Variable());
}

RpcMethod::ParameterError::Enum RpcMethod::checkParameters(std::shared_ptr<std::vector<PVariable>> parameters, std::vector<VariableType> types)
{
	if(types.size() != parameters->size())
	{
		return RpcMethod::ParameterError::Enum::wrongCount;
	}
	for(uint32_t i = 0; i < types.size(); i++)
	{
		if(types.at(i) == VariableType::tVariant && parameters->at(i)->type != VariableType::tVoid) continue;
		if(types.at(i) == VariableType::tInteger && parameters->at(i)->type == VariableType::tInteger64) continue;
		if(types.at(i) == VariableType::tInteger64 && parameters->at(i)->type == VariableType::tInteger) continue;
		if(types.at(i) != parameters->at(i)->type) return RpcMethod::ParameterError::Enum::wrongType;
	}
	return RpcMethod::ParameterError::Enum::noError;
}

RpcMethod::ParameterError::Enum RpcMethod::checkParameters(std::shared_ptr<std::vector<PVariable>> parameters, std::vector<std::vector<VariableType>> types)
{
	RpcMethod::ParameterError::Enum error = RpcMethod::ParameterError::Enum::wrongCount;
	for(std::vector<std::vector<VariableType>>::iterator i = types.begin(); i != types.end(); ++i)
	{
		RpcMethod::ParameterError::Enum result = checkParameters(parameters, *i);
		if(result == RpcMethod::ParameterError::Enum::noError) return result;
		if(result != RpcMethod::ParameterError::Enum::wrongCount) error = result; //Priority of type error is higher than wrong count
	}
	return error;
}

PVariable RpcMethod::getError(RpcMethod::ParameterError::Enum error)
{
	if(error == ParameterError::Enum::wrongCount) return Variable::createError(-1, "Wrong parameter count.");
	else if(error == ParameterError::Enum::wrongType) return Variable::createError(-1, "Type error.");
	return Variable::createError(-1, "Unknown parameter error.");
}

void RpcMethod::setHelp(std::string help)
{
	_help.reset(new Variable(help));
}

void RpcMethod::addSignature(VariableType returnType, std::vector<VariableType> parameterTypes)
{
	if(!_signatures) _signatures.reset(new Variable(VariableType::tArray));

	PVariable element(new Variable(VariableType::tArray));

	element->arrayValue->push_back(PVariable(new Variable(Variable::getTypeString(returnType))));

	for(std::vector<VariableType>::iterator i = parameterTypes.begin(); i != parameterTypes.end(); ++i)
	{
		element->arrayValue->push_back(PVariable(new Variable(Variable::getTypeString(*i))));
	}
	_signatures->arrayValue->push_back(element);
}

}
}

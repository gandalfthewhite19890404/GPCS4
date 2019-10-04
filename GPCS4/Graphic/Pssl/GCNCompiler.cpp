#include "GCNCompiler.h"
#include "../Gnm/GnmSharpBuffer.h"
#include "Platform/UtilString.h"
#include <array>

namespace pssl
{;

constexpr uint32_t PerVertex_Position = 0;
constexpr uint32_t PerVertex_CullDist = 1;
constexpr uint32_t PerVertex_ClipDist = 2;


GCNCompiler::GCNCompiler(
	const PsslProgramInfo& progInfo, 
	const GcnAnalysisInfo& analysis, 
	const GcnShaderInput& shaderInput):
	m_programInfo(progInfo),
	m_analysis(&analysis),
	m_shaderInput(shaderInput)
{
	// Declare an entry point ID. We'll need it during the
	// initialization phase where the execution mode is set.
	m_entryPointId = m_module.allocateId();

	// Set the shader name so that we recognize it in renderdoc
	m_module.setDebugSource(
		spv::SourceLanguageUnknown, 0,
		m_module.addDebugString(progInfo.key().toString().c_str()),
		nullptr);

	//// Set the memory model. This is the same for all shaders.
	m_module.setMemoryModel(
		spv::AddressingModelLogical,
		spv::MemoryModelGLSL450);


	emitInit();
}

GCNCompiler::~GCNCompiler()
{
}

void GCNCompiler::processInstruction(GCNInstruction& ins)
{
	Instruction::InstructionCategory insCategory = ins.instruction->GetInstructionCategory();
	switch (insCategory)
	{
	case Instruction::ScalarALU:
		emitScalarALU(ins);
		break;
	case Instruction::ScalarMemory:
		emitScalarMemory(ins);
		break;
	case Instruction::VectorALU:
		emitVectorALU(ins);
		break;
	case Instruction::VectorMemory:
		emitVectorMemory(ins);
		break;
	case Instruction::FlowControl:
		emitFlowControl(ins);
		break;
	case Instruction::DataShare:
		emitDataShare(ins);
		break;
	case Instruction::VectorInterpolation:
		emitVectorInterpolation(ins);
		break;
	case Instruction::Export:
		emitExport(ins);
		break;
	case Instruction::DebugProfile:
		emitDebugProfile(ins);
		break;
	case Instruction::CategoryUnknown:
	case Instruction::InstructionsCategoriesCount:
		LOG_FIXME("Instruction category not initialized. Encoding %d", ins.instruction->GetInstructionFormat());
		break;
	default:
		break;
	}
}

RcPtr<gve::GveShader> GCNCompiler::finalize()
{
	switch (m_programInfo.shaderType())
	{
	case VertexShader:   this->emitVsFinalize(); break;
	case HullShader:     this->emitHsFinalize(); break;
	case DomainShader:   this->emitDsFinalize(); break;
	case GeometryShader: this->emitGsFinalize(); break;
	case PixelShader:    this->emitPsFinalize(); break;
	case ComputeShader:  this->emitCsFinalize(); break;
	}

	// Declare the entry point, we now have all the
	// information we need, including the interfaces
	m_module.addEntryPoint(m_entryPointId,
		m_programInfo.executionModel(), "main",
		m_entryPointInterfaces.size(),
		m_entryPointInterfaces.data());
	m_module.setDebugName(m_entryPointId, "main");

	return new gve::GveShader(m_programInfo.shaderStage(),
		m_module.compile(),
		m_programInfo.key());
}


void GCNCompiler::emitInit()
{
	// Set up common capabilities for all shaders
	m_module.enableCapability(spv::CapabilityShader);
	m_module.enableCapability(spv::CapabilityImageQuery);

	// Initialize the shader module with capabilities
	// etc. Each shader type has its own peculiarities.
	switch (m_programInfo.shaderType())
	{
	case VertexShader:   emitVsInit(); break;
	case HullShader:     emitHsInit(); break;
	case DomainShader:   emitDsInit(); break;
	case GeometryShader: emitGsInit(); break;
	case PixelShader:    emitPsInit(); break;
	case ComputeShader:  emitCsInit(); break;
	}
}

void GCNCompiler::emitVsInit()
{
	//m_module.enableCapability(spv::CapabilityClipDistance);
	//m_module.enableCapability(spv::CapabilityCullDistance);
	m_module.enableCapability(spv::CapabilityDrawParameters);

	m_module.enableExtension("SPV_KHR_shader_draw_parameters");

	emitDclVertexInput();
	emitDclVertexOutput();
	emitDclUniformBuffer();
	emitEmuFetchShader();
	

	// Main function of the vertex shader
	m_vs.mainFunctionId = m_module.allocateId();
	m_module.setDebugName(m_vs.mainFunctionId, "vsMain");

	emitFunctionBegin(
		m_vs.mainFunctionId,
		m_module.defVoidType(),
		m_module.defFunctionType(
		m_module.defVoidType(), 0, nullptr));

	emitFunctionLabel();

	m_module.opFunctionCall(
		m_module.defVoidType(),
		m_vs.fsFunctionId, 0, nullptr);
}

void GCNCompiler::emitHsInit()
{

}

void GCNCompiler::emitDsInit()
{

}

void GCNCompiler::emitGsInit()
{

}

void GCNCompiler::emitPsInit()
{

}

void GCNCompiler::emitCsInit()
{

}

void GCNCompiler::emitVsFinalize()
{
	this->emitMainFunctionBegin();

	//emitInputSetup();

	m_module.opFunctionCall(
		m_module.defVoidType(),
		m_vs.mainFunctionId, 0, nullptr);

	//emitOutputSetup();
	
	this->emitFunctionEnd();
}

void GCNCompiler::emitHsFinalize()
{

}

void GCNCompiler::emitDsFinalize()
{

}

void GCNCompiler::emitGsFinalize()
{

}

void GCNCompiler::emitPsFinalize()
{

}

void GCNCompiler::emitCsFinalize()
{

}

void GCNCompiler::emitFunctionBegin(uint32_t entryPoint, uint32_t returnType, uint32_t funcType)
{
	emitFunctionEnd();

	m_module.functionBegin(
		returnType, entryPoint, funcType,
		spv::FunctionControlMaskNone);

	m_insideFunction = true;
}

void GCNCompiler::emitFunctionEnd()
{
	if (m_insideFunction) 
	{
		m_module.opReturn();
		m_module.functionEnd();
	}

	m_insideFunction = false;
}

void GCNCompiler::emitMainFunctionBegin()
{
	emitFunctionBegin(
		m_entryPointId,
		m_module.defVoidType(),
		m_module.defFunctionType(
		m_module.defVoidType(), 0, nullptr));

	emitFunctionLabel();
}

void GCNCompiler::emitFunctionLabel()
{
	m_module.opLabel(m_module.allocateId());
}

void GCNCompiler::emitDclVertexInput()
{
	do 
	{
		if (!m_shaderInput.vsInputSemantics.has_value())
		{
			break;
		}

		for (const auto& inputSemantic : m_shaderInput.vsInputSemantics.value())
		{
			// TODO:
			// Not sure if all vertex inputs are float type
			auto inputReg = emitDclFloatVectorVar(SpirvScalarType::Float32, inputSemantic.sizeInElements, spv::StorageClassInput);
			m_vs.vsInputs[inputSemantic.semantic] = inputReg;
			m_module.setDebugName(inputReg.id, 
				UtilString::Format("inParam%d", inputSemantic.semantic).c_str());

			// Use semantic index for location, so vulkan code need to match.
			m_module.decorateLocation(inputReg.id, inputSemantic.semantic);
			m_entryPointInterfaces.push_back(inputReg.id);
		}
	} while (false);
}

void GCNCompiler::emitDclVertexOutput()
{
	// Declare the per-vertex output block. This is where
	// the vertex shader will write the vertex position.
	const uint32_t perVertexStructType = getPerVertexBlockId();
	const uint32_t perVertexPointerType = m_module.defPointerType(
		perVertexStructType, spv::StorageClassOutput);

	m_perVertexOut = m_module.newVar(perVertexPointerType, spv::StorageClassOutput);

	m_entryPointInterfaces.push_back(m_perVertexOut);
	m_module.setDebugName(m_perVertexOut, "vsVertexOut");

	// Declare other vertex output.
	// like normal or texture coordinate
	do 
	{
		uint32_t outLocation = 0;
		for (const auto& expInfo : m_analysis->expParams)
		{
			if (expInfo.target == EXPInstruction::TGT::TGTExpPosMin)
			{
				// Already handled above
				continue;
			}

			auto outVector = emitDclFloatVectorVar(SpirvScalarType::Float32,
				expInfo.regIndices.size(),
				spv::StorageClassOutput,
				UtilString::Format("outParam%d", outLocation));
			m_module.decorateLocation(outVector.id, outLocation);

			m_vs.vsOutputs[expInfo.target] = outVector;

			++outLocation;
		}
	} while (false);
	
}

void GCNCompiler::emitEmuFetchShader()
{
	do 
	{
		if (!m_shaderInput.vsInputSemantics.has_value())
		{
			break;
		}

		m_vs.fsFunctionId = m_module.allocateId();

		emitFunctionBegin(
			m_vs.fsFunctionId,
			m_module.defVoidType(),
			m_module.defFunctionType(
			m_module.defVoidType(), 0, nullptr));
		emitFunctionLabel();
		m_module.setDebugName(m_vs.fsFunctionId, "vsFetch");

		for (const auto& inputSemantic : m_shaderInput.vsInputSemantics.value())
		{
			for (uint32_t i = 0; i != inputSemantic.sizeInElements; ++i)
			{
				uint32_t vgprIdx = inputSemantic.vgpr + i;

				// Declare a new vgpr reg
				// TODO:
				// Not sure if all vertex inputs are float type
				auto vgprReg = emitDclFloat(SpirvScalarType::Float32, 
					spv::StorageClassPrivate, UtilString::Format("v%d", vgprIdx));
				uint32_t inputVarId = m_vs.vsInputs[inputSemantic.semantic].id;

				// Access vector member
				uint32_t fpPtrTypeId = m_module.defFloatPointerType(32, spv::StorageClassPrivate);
				uint32_t accessIndexArray[] = { m_module.constu32(i) };
				uint32_t inputElementId = m_module.opAccessChain(
					fpPtrTypeId,
					inputVarId,
					1, accessIndexArray);

				// Store input value to our new vgpr reg.
				uint32_t loadId = m_module.opLoad(fpPtrTypeId, inputElementId);
				m_module.opStore(vgprReg.id, loadId);

				// Save to the map
				m_vgprs[vgprIdx] = vgprReg;
			}
		}

		emitFunctionEnd();
	} while (false);
}

void GCNCompiler::emitDclUniformBuffer()
{
	// For PSSL uniform buffer, it's hard to detect how many variables have been declared,
	// and even if we know, it's almost useless, because the shader could access part of a variable,
	// like the upper-left mat3x3 of a mat4x4, thus can't be accessed via AccessChain.
	// So here we treat all the uniform buffer together as a dword array.
	// Then we can access any element in this array.
	
	// Based on the above, there're at least 2 ways to achieve this.
	// First is UBO and second is SSBO.
	// 
	// 1. For UBO, the disadvantage is that we can not declare a variable-length array of UBO member,
	// thus we have to get this information from stride field of input V# buffer,
	// which make things a little more complicated.
	// But what we gain is performance, UBO access is usually faster than SSBO.
	// 
	// 2. For SSBO, the advantage is that it support variable-length arrays, which will make
	// the compiler implementation a little easier. 
	// Besides, SSBO support write to the buffer,
	// The disadvantage is that it will slower than UBO.
	// 
	// Currently I can not determine which one is better, and how much performance we could gain from using UBO,
	// but I just choose the UBO way first due to performance reason. Maybe need to change in the future.

	uint32_t index = 0;
	for (const auto& res : m_shaderInput.resourceBuffer)
	{
		switch (res.type)
		{
		case SpirvResourceType::VSharp:
		{
			GnmBuffer* vsharpBuffer = reinterpret_cast<GnmBuffer*>(res.res.resource);
			uint32_t arraySize = vsharpBuffer->stride / sizeof(uint32_t);

			uint32_t arrayId = m_module.defArrayTypeUnique(
				m_module.defFloatType(32),
				m_module.constu32(arraySize));
			m_module.decorateArrayStride(arrayId, vsharpBuffer->stride);
			uint32_t uboStuctId = m_module.defStructTypeUnique(1, &arrayId);
			m_module.decorateBlock(uboStuctId);
			m_module.memberDecorateOffset(uboStuctId, 0, 0);
			m_module.setDebugName(uboStuctId, "UniformBufferObject");
			m_module.setDebugMemberName(uboStuctId, 0, "data");

			uint32_t uboPtrId = m_module.defPointerType(uboStuctId, spv::StorageClassUniform);
			m_uboId = m_module.newVar(uboPtrId, spv::StorageClassUniform);

			// TODO:
			// Not sure, need to correct.
			m_module.decorateDescriptorSet(m_uboId, index);
			m_module.decorateBinding(m_uboId, index);

			m_module.setDebugName(m_uboId, "ubo");
			
		}
			break;
		case SpirvResourceType::SSharp:
			break;
		case SpirvResourceType::TSharp:
			break;
		default:
			break;
		}

		++index;
	}
	
}

void GCNCompiler::emitDclImmConstBuffer(const InputUsageSlot* usageSlot)
{
	
}

void GCNCompiler::emitDclImmSampler(const InputUsageSlot* usageSlot)
{

}

SpirvRegisterPointer GCNCompiler::emitDclFloat(SpirvScalarType type,
	spv::StorageClass storageCls, const std::string& debugName /*= ""*/)
{
	uint32_t width = type == SpirvScalarType::Float32 ? 32 : 64;
	uint32_t fpPtrTypeId = m_module.defFloatPointerType(width, storageCls);
	uint32_t varId = m_module.newVar(fpPtrTypeId, storageCls);
	if (!debugName.empty())
	{
		m_module.setDebugName(varId, debugName.c_str());
	}
	
	return SpirvRegisterPointer(type, 1, varId);
}

SpirvRegisterPointer GCNCompiler::emitDclFloatVectorType(SpirvScalarType type, uint32_t count,
	spv::StorageClass storageCls, const std::string& debugName /*= ""*/)
{
	uint32_t width = type == SpirvScalarType::Float32 ? 32 : 64;
	uint32_t fpTypeId = m_module.defFloatType(width);
	uint32_t vfpTypeId = m_module.defVectorType(fpTypeId, count);
	uint32_t vfpPtrTypeId = m_module.defPointerType(vfpTypeId, storageCls);
	
	if (!debugName.empty())
	{
		m_module.setDebugName(vfpPtrTypeId, debugName.c_str());
	}
	return SpirvRegisterPointer(type, count, vfpPtrTypeId);
}

SpirvRegisterPointer GCNCompiler::emitDclFloatVectorVar(SpirvScalarType type, uint32_t count, spv::StorageClass storageCls, const std::string& debugName /*= ""*/)
{
	auto ptrType = emitDclFloatVectorType(type, count, storageCls, debugName);
	uint32_t varId = m_module.newVar(ptrType.id, storageCls);
	if (!debugName.empty())
	{
		m_module.setDebugName(varId, debugName.c_str());
	}
	return SpirvRegisterPointer(type, count, varId);
}

SpirvRegisterValue GCNCompiler::emitValueLoad(const SpirvRegisterPointer& reg)
{
	uint32_t varId = m_module.opLoad(
		getVectorTypeId(reg.type),
		reg.id);
	return SpirvRegisterValue(reg.type, varId);
}

SpirvRegisterValue GCNCompiler::emitSgprLoad(uint32_t index)
{
	return emitValueLoad(m_sgprs[index]);
}

SpirvRegisterValue GCNCompiler::emitVgprLoad(uint32_t index)
{
	return emitValueLoad(m_vgprs[index]);
}

void GCNCompiler::emitValueStore(
	const SpirvRegisterPointer &ptr,
	const SpirvRegisterValue &src, 
	const GcnRegMask &writeMask)
{
	SpirvRegisterValue value = src;
	// If the component types are not compatible,
	// we need to bit-cast the source variable.
	if (src.type.ctype != ptr.type.ctype)
	{
		value = emitRegisterBitcast(src, ptr.type.ctype);
	}
		
	// If the source value consists of only one component,
	// it is stored in all components of the destination.
	if (src.type.ccount == 1)
	{
		value = emitRegisterExtend(src, writeMask.popCount());
	}
		
	if (ptr.type.ccount == writeMask.popCount()) 
	{
		// Simple case: We write to the entire register
		m_module.opStore(ptr.id, value.id);
	}
	else 
	{
		// We only write to part of the destination
		// register, so we need to load and modify it
		SpirvRegisterValue tmp = emitValueLoad(ptr);
		tmp = emitRegisterInsert(tmp, value, writeMask);

		m_module.opStore(ptr.id, tmp.id);
	}
}

void GCNCompiler::emitSgprStore(uint32_t dstIdx, const SpirvRegisterValue& srcReg)
{
	auto& sgpr = m_sgprs[dstIdx];
	if (sgpr.id == 0)  // Not initialized
	{
		sgpr.type = srcReg.type;
		// TODO:
		// Not sure whether the storage class should be Function, maybe Private is better?
		sgpr.id = m_module.newVar(getVectorTypeId(srcReg.type), spv::StorageClassFunction);
		m_module.setDebugName(sgpr.id, UtilString::Format("s%d", dstIdx).c_str());
	}
	emitValueStore(sgpr, srcReg, 1);
}

void GCNCompiler::emitSgprArrayStore(uint32_t startIdx, const SpirvRegisterValue* values, uint32_t count)
{
	for (uint32_t i = 0; i != count; ++i)
	{
		emitSgprStore(startIdx + i, values[i]);
	}
}

void GCNCompiler::emitVgprStore(uint32_t dstIdx, const SpirvRegisterValue& srcReg)
{
	auto& vgpr = m_vgprs[dstIdx];
	if (vgpr.id == 0)  // Not initialized
	{
		vgpr.type = srcReg.type;
		// TODO:
		// Not sure whether the storage class should be Function, maybe Private is better?
		vgpr.id = m_module.newVar(getVectorTypeId(srcReg.type), spv::StorageClassFunction);
		m_module.setDebugName(vgpr.id, UtilString::Format("v%d", dstIdx).c_str());
	}
	emitValueStore(vgpr, srcReg, 1);
}

void GCNCompiler::emitVgprArrayStore(uint32_t startIdx, const SpirvRegisterValue* values, uint32_t count)
{
	for (uint32_t i = 0; i != count; ++i)
	{
		emitVgprStore(startIdx + i, values[i]);
	}
}

// Used with with 7 bits SDST, 8 bits SSRC or 9 bits SRC
// See table "SDST, SSRC and SRC Operands" in section 3.1 of GPU Shader Core ISA manual
pssl::SpirvRegisterValue GCNCompiler::emitLoadScalarOperand(uint32_t srcOperand, uint32_t regIndex, uint32_t literalConst /*= 0*/)
{
	Instruction::OperandSRC src = static_cast<Instruction::OperandSRC>(srcOperand);
	SpirvRegisterValue operand;
	
	switch (src)
	{
	case Instruction::OperandSRC::SRCScalarGPRMin ... Instruction::OperandSRC::SRCScalarGPRMax:
	{
		operand = emitSgprLoad(regIndex);
	}
		break;
	case Instruction::OperandSRC::SRCVccLo:
		break;
	case Instruction::OperandSRC::SRCVccHi:
		break;
	case Instruction::OperandSRC::SRCM0:
		break;
	case Instruction::OperandSRC::SRCExecLo:
		break;
	case Instruction::OperandSRC::SRCExecHi:
		break;
	case Instruction::OperandSRC::SRCConstZero:
	case Instruction::OperandSRC::SRCSignedConstIntPosMin ... Instruction::OperandSRC::SRCSignedConstIntPosMax:
	case Instruction::OperandSRC::SRCSignedConstIntNegMin ... Instruction::OperandSRC::SRCSignedConstIntNegMax:
		operand = emitInlineConstantInteger(src);
		break;
	case Instruction::OperandSRC::SRCConstFloatPos_0_5 ... Instruction::OperandSRC::SRCConstFloatNeg_4_0:
		operand = emitInlineConstantFloat(src);
		break;
	case Instruction::OperandSRC::SRCVCCZ:
		break;
	case Instruction::OperandSRC::SRCEXECZ:
		break;
	case Instruction::OperandSRC::SRCSCC:
		break;
	case Instruction::OperandSRC::SRCLdsDirect:
		break;
	case Instruction::OperandSRC::SRCLiteralConst:
	{
		uint32_t constId = m_module.constu32(literalConst);
		operand = SpirvRegisterValue(SpirvScalarType::Uint32, 1, constId);

		m_constValueTable[constId] = SpirvLiteralConstant(operand.type.ctype, literalConst);
	}
		break;
	// For 9 bits SRC operand
	case Instruction::OperandSRC::SRCVectorGPRMin ... Instruction::OperandSRC::SRCVectorGPRMax:
	{

	}
		break;
	default:
		LOG_ERR("error operand range %d", (uint32_t)srcOperand);
		break;
	}

	return operand;
}

// Used with 8 bits VSRC/VDST
// for 9 bits SRC, call emitLoadScalarOperand instead
// See table "VSRC and VDST Operands" in section 3.1 of GPU Shader Core ISA manual
SpirvRegisterValue GCNCompiler::emitLoadVectorOperand(uint32_t index)
{

}

// Used with 7 bits SDST
void GCNCompiler::emitStoreScalarOperand(uint32_t dstOperand, uint32_t regIndex, const SpirvRegisterValue& srcReg)
{
	Instruction::OperandSDST dst = static_cast<Instruction::OperandSDST>(dstOperand);
	
	switch (dst)
	{
	case Instruction::OperandSDST::SDSTScalarGPRMin ... Instruction::OperandSDST::SDSTScalarGPRMax:
	{
		emitSgprStore(regIndex, srcReg);
	}
		break;
	case Instruction::OperandSDST::SDSTVccLo:
		emitStoreVCC(srcReg, false);
		break;
	case Instruction::OperandSDST::SDSTVccHi:
		emitStoreVCC(srcReg, true);
		break;
	case Instruction::OperandSDST::SDSTM0:
		emitStoreM0(srcReg);
		break;
	case Instruction::OperandSDST::SDSTExecLo:
		break;
	case Instruction::OperandSDST::SDSTExecHi:
		break;
	default:
		LOG_ERR("error operand range %d", (uint32_t)dst);
		break;
	}
}

// Used with 8 bits VSRC/VDST
// for 9 bits SRC, call emitLoadScalarOperand instead
// See table "VSRC and VDST Operands" in section 3.1 of GPU Shader Core ISA manual
void GCNCompiler::emitStoreVectorOperand(uint32_t dstIndex, const SpirvRegisterValue& srcReg)
{
	emitVgprStore(dstIndex, srcReg);
}

SpirvRegisterValue GCNCompiler::emitInlineConstantFloat(Instruction::OperandSRC src)
{
	float value = 0.0;
	switch (src)
	{
	case Instruction::OperandSRC::SRCConstFloatPos_0_5:
		value = 0.5;
		break;
	case Instruction::OperandSRC::SRCConstFloatNeg_0_5:
		value = -0.5;
		break;
	case Instruction::OperandSRC::SRCConstFloatPos_1_0:
		value = 1.0;
		break;
	case Instruction::OperandSRC::SRCConstFloatNeg_1_0:
		value = -1.0;
		break;
	case Instruction::OperandSRC::SRCConstFloatPos_2_0:
		value = 2.0;
		break;
	case Instruction::OperandSRC::SRCConstFloatNeg_2_0:
		value = -2.0;
		break;
	case Instruction::OperandSRC::SRCConstFloatPos_4_0:
		value = 4.0;
		break;
	case Instruction::OperandSRC::SRCConstFloatNeg_4_0:
		value = -4.0;
		break;
	default:
		break;
	}

	uint32_t valueId = m_module.constf32(value);
	return SpirvRegisterValue(SpirvScalarType::Float32, 1, valueId);
}

SpirvRegisterValue GCNCompiler::emitInlineConstantInteger(Instruction::OperandSRC src)
{
	int32_t value = 0;
	switch (src)
	{
	case Instruction::OperandSRC::SRCConstZero:
		value = 0;
		break;
	case Instruction::OperandSRC::SRCSignedConstIntPosMin ... Instruction::OperandSRC::SRCSignedConstIntPosMax:
		value = (int32_t)src - 128;
		break;
	case Instruction::OperandSRC::SRCSignedConstIntNegMin ... Instruction::OperandSRC::SRCSignedConstIntNegMax:
		value = 192 - (int32_t)src;
		break;
	default:
		break;
	}

	uint32_t valueId = m_module.consti32(value);
	return SpirvRegisterValue(SpirvScalarType::Sint32, 1, valueId);
}

void GCNCompiler::emitStoreVCC(const SpirvRegisterValue& vccValueReg, bool isVccHi)
{
	do 
	{
		const auto& spvConst = m_constValueTable[vccValueReg.id];
		if (spvConst.type != SpirvScalarType::Unknown)
		{
			// Vcc source is an immediate constant value.
			uint32_t vccValue = spvConst.literalConst;
			// TODO:
			// Change VCC will change hardware state accordingly.
			// Currently I just record the value and do nothing.
			m_stateRegs.vcc = isVccHi ? (uint64_t(vccValue) << 32) : vccValue;
		}
		else
		{
			// Vcc source is a register.
		}

	} while (false);

}

void GCNCompiler::emitStoreM0(const SpirvRegisterValue& m0ValueReg)
{
	// M0 is used by several types of instruction for accessing LDS or GDS, 
	// for indirect GPR addressing and for sending messages to VGT.
	// But if there's no such instruction in current shader,
	// it will be used for debugging purpose, together with s_ttracedata

	const auto& spvConst = m_constValueTable[m0ValueReg.id];
	if (spvConst.type != SpirvScalarType::Unknown)
	{
		// M0 source is an immediate constant value.

		// TODO:
		// Change M0 will change hardware state accordingly.
		// Currently I just record the value and do nothing.
		m_stateRegs.m0 = spvConst.literalConst;
	}
	else
	{
		// M0 source is a register.
	}
}

pssl::SpirvRegisterValue GCNCompiler::emitBuildConstVecf32(float x, float y, float z, float w, const GcnRegMask& writeMask)
{
	// TODO refactor these functions into one single template
	std::array<uint32_t, 4> ids = { 0, 0, 0, 0 };
	uint32_t componentIndex = 0;

	if (writeMask[0]) ids[componentIndex++] = m_module.constf32(x);
	if (writeMask[1]) ids[componentIndex++] = m_module.constf32(y);
	if (writeMask[2]) ids[componentIndex++] = m_module.constf32(z);
	if (writeMask[3]) ids[componentIndex++] = m_module.constf32(w);

	SpirvRegisterValue result;
	result.type.ctype = SpirvScalarType::Float32;
	result.type.ccount = componentIndex;
	result.id = componentIndex > 1
		? m_module.constComposite(
			getVectorTypeId(result.type),
			componentIndex, ids.data())
		: ids[0];
	return result;
}

pssl::SpirvRegisterValue GCNCompiler::emitBuildConstVecu32(uint32_t x, uint32_t y, uint32_t z, uint32_t w, const GcnRegMask& writeMask)
{
	std::array<uint32_t, 4> ids = { 0, 0, 0, 0 };
	uint32_t componentIndex = 0;

	if (writeMask[0]) ids[componentIndex++] = m_module.constu32(x);
	if (writeMask[1]) ids[componentIndex++] = m_module.constu32(y);
	if (writeMask[2]) ids[componentIndex++] = m_module.constu32(z);
	if (writeMask[3]) ids[componentIndex++] = m_module.constu32(w);

	SpirvRegisterValue result;
	result.type.ctype = SpirvScalarType::Uint32;
	result.type.ccount = componentIndex;
	result.id = componentIndex > 1
		? m_module.constComposite(
			getVectorTypeId(result.type),
			componentIndex, ids.data())
		: ids[0];
	return result;
}

pssl::SpirvRegisterValue GCNCompiler::emitBuildConstVeci32(int32_t x, int32_t y, int32_t z, int32_t w, const GcnRegMask& writeMask)
{
	std::array<uint32_t, 4> ids = { 0, 0, 0, 0 };
	uint32_t componentIndex = 0;

	if (writeMask[0]) ids[componentIndex++] = m_module.consti32(x);
	if (writeMask[1]) ids[componentIndex++] = m_module.consti32(y);
	if (writeMask[2]) ids[componentIndex++] = m_module.consti32(z);
	if (writeMask[3]) ids[componentIndex++] = m_module.consti32(w);

	SpirvRegisterValue result;
	result.type.ctype = SpirvScalarType::Sint32;
	result.type.ccount = componentIndex;
	result.id = componentIndex > 1
		? m_module.constComposite(
			getVectorTypeId(result.type),
			componentIndex, ids.data())
		: ids[0];
	return result;
}

pssl::SpirvRegisterValue GCNCompiler::emitBuildConstVecf64(double xy, double zw, const GcnRegMask& writeMask)
{
	std::array<uint32_t, 2> ids = { 0, 0 };
	uint32_t componentIndex = 0;

	if (writeMask[0] && writeMask[1]) ids[componentIndex++] = m_module.constf64(xy);
	if (writeMask[2] && writeMask[3]) ids[componentIndex++] = m_module.constf64(zw);

	SpirvRegisterValue result;
	result.type.ctype = SpirvScalarType::Float64;
	result.type.ccount = componentIndex;
	result.id = componentIndex > 1
		? m_module.constComposite(
			getVectorTypeId(result.type),
			componentIndex, ids.data())
		: ids[0];
	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterBitcast(SpirvRegisterValue srcValue, SpirvScalarType dstType)
{
	SpirvScalarType srcType = srcValue.type.ctype;

	if (srcType == dstType)
		return srcValue;

	SpirvRegisterValue result;
	result.type.ctype = dstType;
	result.type.ccount = srcValue.type.ccount;

	if (isWideType(srcType)) result.type.ccount *= 2;
	if (isWideType(dstType)) result.type.ccount /= 2;

	result.id = m_module.opBitcast(
		getVectorTypeId(result.type),
		srcValue.id);
	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterSwizzle(SpirvRegisterValue value, GcnRegSwizzle swizzle, GcnRegMask writeMask)
{
	if (value.type.ccount == 1)
	{
		return emitRegisterExtend(value, writeMask.popCount());
	}
		
	std::array<uint32_t, 4> indices;

	uint32_t dstIndex = 0;

	for (uint32_t i = 0; i < 4; i++) 
	{
		if (writeMask[i])
		{
			indices[dstIndex++] = swizzle[i];
		}
	}

	// If the swizzle combined with the mask can be reduced
	// to a no-op, we don't need to insert any instructions.
	bool isIdentitySwizzle = dstIndex == value.type.ccount;

	for (uint32_t i = 0; i < dstIndex && isIdentitySwizzle; i++)
		isIdentitySwizzle &= indices[i] == i;

	if (isIdentitySwizzle)
	{
		return value;
	}

	// Use OpCompositeExtract if the resulting vector contains
	// only one component, and OpVectorShuffle if it is a vector.
	SpirvRegisterValue result;
	result.type.ctype = value.type.ctype;
	result.type.ccount = dstIndex;

	const uint32_t typeId = getVectorTypeId(result.type);

	if (dstIndex == 1) 
	{
		result.id = m_module.opCompositeExtract(
			typeId, value.id, 1, indices.data());
	}
	else 
	{
		result.id = m_module.opVectorShuffle(
			typeId, value.id, value.id,
			dstIndex, indices.data());
	}

	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterExtract(SpirvRegisterValue value, GcnRegMask mask)
{
	return emitRegisterSwizzle(value,
		GcnRegSwizzle(0, 1, 2, 3), mask);
}

SpirvRegisterValue GCNCompiler::emitRegisterInsert(SpirvRegisterValue dstValue, SpirvRegisterValue srcValue, GcnRegMask srcMask)
{
	SpirvRegisterValue result;
	result.type = dstValue.type;

	const uint32_t typeId = getVectorTypeId(result.type);

	if (srcMask.popCount() == 0) 
	{
		// Nothing to do if the insertion mask is empty
		result.id = dstValue.id;
	}
	else if (dstValue.type.ccount == 1) 
	{
		// Both values are scalar, so the first component
		// of the write mask decides which one to take.
		result.id = srcMask[0] ? srcValue.id : dstValue.id;
	}
	else if (srcValue.type.ccount == 1) 
	{
		// The source value is scalar. Since OpVectorShuffle
		// requires both arguments to be vectors, we have to
		// use OpCompositeInsert to modify the vector instead.
		const uint32_t componentId = srcMask.firstSet();

		result.id = m_module.opCompositeInsert(typeId,
			srcValue.id, dstValue.id, 1, &componentId);
	}
	else 
	{
		// Both arguments are vectors. We can determine which
		// components to take from which vector and use the
		// OpVectorShuffle instruction.
		std::array<uint32_t, 4> components;
		uint32_t srcComponentId = dstValue.type.ccount;

		for (uint32_t i = 0; i < dstValue.type.ccount; i++)
		{
			components.at(i) = srcMask[i] ? srcComponentId++ : i;
		}
			
		result.id = m_module.opVectorShuffle(
			typeId, dstValue.id, srcValue.id,
			dstValue.type.ccount, components.data());
	}

	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterConcat(SpirvRegisterValue value1, SpirvRegisterValue value2)
{
	std::array<uint32_t, 2> ids =
	{ { value1.id, value2.id } };

	SpirvRegisterValue result;
	result.type.ctype = value1.type.ctype;
	result.type.ccount = value1.type.ccount + value2.type.ccount;
	result.id = m_module.opCompositeConstruct(
		getVectorTypeId(result.type),
		ids.size(), ids.data());
	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterExtend(SpirvRegisterValue value, uint32_t size)
{
	if (size == 1)
	{
		return value;
	}
		
	std::array<uint32_t, 4> ids = { {
	  value.id, value.id,
	  value.id, value.id,
	} };

	SpirvRegisterValue result;
	result.type.ctype = value.type.ctype;
	result.type.ccount = size;
	result.id = m_module.opCompositeConstruct(
		getVectorTypeId(result.type),
		size, ids.data());
	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterAbsolute(SpirvRegisterValue value)
{
	const uint32_t typeId = getVectorTypeId(value.type);

	switch (value.type.ctype)
	{
	case SpirvScalarType::Float32: value.id = m_module.opFAbs(typeId, value.id); break;
	case SpirvScalarType::Sint32:  value.id = m_module.opSAbs(typeId, value.id); break;
	default: LOG_WARN("GCNCompiler: Cannot get absolute value for given type");
	}

	return value;
}

SpirvRegisterValue GCNCompiler::emitRegisterNegate(SpirvRegisterValue value)
{
	const uint32_t typeId = getVectorTypeId(value.type);

	switch (value.type.ctype) 
	{
	case SpirvScalarType::Float32: value.id = m_module.opFNegate(typeId, value.id); break;
	case SpirvScalarType::Float64: value.id = m_module.opFNegate(typeId, value.id); break;
	case SpirvScalarType::Sint32:  value.id = m_module.opSNegate(typeId, value.id); break;
	case SpirvScalarType::Sint64:  value.id = m_module.opSNegate(typeId, value.id); break;
	default: LOG_WARN("GCNCompiler: Cannot negate given type");
	}

	return value;
}

SpirvRegisterValue GCNCompiler::emitRegisterZeroTest(SpirvRegisterValue value, SpirvZeroTest test)
{
	SpirvRegisterValue result;
	result.type.ctype = SpirvScalarType::Bool;
	result.type.ccount = 1;

	const uint32_t zeroId = m_module.constu32(0u);
	const uint32_t typeId = getVectorTypeId(result.type);

	result.id = test == SpirvZeroTest::TestZ
		? m_module.opIEqual(typeId, value.id, zeroId)
		: m_module.opINotEqual(typeId, value.id, zeroId);
	return result;
}

SpirvRegisterValue GCNCompiler::emitRegisterMaskBits(SpirvRegisterValue value, uint32_t mask)
{
	SpirvRegisterValue maskVector = emitBuildConstVecu32(
		mask, mask, mask, mask, GcnRegMask::firstN(value.type.ccount));

	SpirvRegisterValue result;
	result.type = value.type;
	result.id = m_module.opBitwiseAnd(
		getVectorTypeId(result.type),
		value.id, maskVector.id);
	return result;
}

uint32_t GCNCompiler::getPerVertexBlockId()
{
	// Should be:
	// out gl_PerVertex
	// {
	//   vec4 gl_Position;
	//   float gl_PointSize;
	//   float gl_ClipDist[];
	//   float gl_CullDist[];
	// };

	uint32_t t_f32 = m_module.defFloatType(32);
	uint32_t t_f32_v4 = m_module.defVectorType(t_f32, 4);
	//     uint32_t t_f32_a4 = m_module.defArrayType(t_f32, m_module.constu32(4));

	std::array<uint32_t, 1> members;
	members[PerVertex_Position] = t_f32_v4;
	//     members[PerVertex_CullDist] = t_f32_a4;
	//     members[PerVertex_ClipDist] = t_f32_a4;

	uint32_t typeId = m_module.defStructTypeUnique(
		members.size(), members.data());

	m_module.memberDecorateBuiltIn(typeId, PerVertex_Position, spv::BuiltInPosition);
	//     m_module.memberDecorateBuiltIn(typeId, PerVertex_CullDist, spv::BuiltInCullDistance);
	//     m_module.memberDecorateBuiltIn(typeId, PerVertex_ClipDist, spv::BuiltInClipDistance);
	m_module.decorateBlock(typeId);

	m_module.setDebugName(typeId, "gl_PerVertex");
	m_module.setDebugMemberName(typeId, PerVertex_Position, "gl_Position");
	//     m_module.setDebugMemberName(typeId, PerVertex_CullDist, "cull_dist");
	//     m_module.setDebugMemberName(typeId, PerVertex_ClipDist, "clip_dist");
	return typeId;
}

uint32_t GCNCompiler::getScalarTypeId(SpirvScalarType type)
{
	if (type == SpirvScalarType::Float64)
	{
		m_module.enableCapability(spv::CapabilityFloat64);
	}

	if (type == SpirvScalarType::Sint64 || type == SpirvScalarType::Uint64)
	{
		m_module.enableCapability(spv::CapabilityInt64);
	}

	uint32_t typeId = 0;
	switch (type) 
	{
	case SpirvScalarType::Uint32:  typeId = m_module.defIntType(32, 0); break;
	case SpirvScalarType::Uint64:  typeId = m_module.defIntType(64, 0); break;
	case SpirvScalarType::Sint32:  typeId = m_module.defIntType(32, 1); break;
	case SpirvScalarType::Sint64:  typeId = m_module.defIntType(64, 1); break;
	case SpirvScalarType::Float32: typeId = m_module.defFloatType(32); break;
	case SpirvScalarType::Float64: typeId = m_module.defFloatType(64); break;
	case SpirvScalarType::Bool:    typeId = m_module.defBoolType(); break;
	}
	return typeId;
}

uint32_t GCNCompiler::getVectorTypeId(const SpirvVectorType& type)
{
	uint32_t typeId = this->getScalarTypeId(type.ctype);

	if (type.ccount > 1)
	{
		typeId = m_module.defVectorType(typeId, type.ccount);
	}

	return typeId;
}

bool GCNCompiler::isWideType(SpirvScalarType type) const
{
	return type == SpirvScalarType::Sint64
		|| type == SpirvScalarType::Uint64
		|| type == SpirvScalarType::Float64;
}

} // namespace pssl
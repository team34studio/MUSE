// McpTool_ManageAI.cpp — manage_ai tool definition (60 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageAI : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_ai"); }

	FString GetDescription() const override
	{
		return TEXT("Create AI Controllers, configure Behavior Trees, Blackboards, "
			"EQS queries, perception systems, Behavior Tree graphs, and navigation.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_ai_controller"),
				TEXT("assign_behavior_tree"),
				TEXT("assign_blackboard"),
				TEXT("create_blackboard_asset"),
				TEXT("add_blackboard_key"),
				TEXT("set_key_instance_synced"),
				TEXT("create_behavior_tree"),
				TEXT("add_composite_node"),
				TEXT("add_task_node"),
				TEXT("add_decorator"),
				TEXT("add_service"),
				TEXT("configure_bt_node"),
				TEXT("create_eqs_query"),
				TEXT("add_eqs_generator"),
				TEXT("add_eqs_context"),
				TEXT("add_eqs_test"),
				TEXT("configure_test_scoring"),
				TEXT("add_ai_perception_component"),
				TEXT("configure_sight_config"),
				TEXT("configure_hearing_config"),
				TEXT("configure_damage_sense_config"),
				TEXT("set_perception_team"),
				TEXT("create_state_tree"),
				TEXT("add_state_tree_state"),
				TEXT("add_state_tree_transition"),
				TEXT("configure_state_tree_task"),
				TEXT("create_smart_object_definition"),
				TEXT("add_smart_object_slot"),
				TEXT("configure_slot_behavior"),
				TEXT("add_smart_object_component"),
				TEXT("create_mass_entity_config"),
				TEXT("configure_mass_entity"),
				TEXT("add_mass_spawner"),
				TEXT("get_ai_info"),
				TEXT("create_blackboard"),
				TEXT("setup_perception"),
				TEXT("create_nav_link_proxy"),
				TEXT("set_focus"),
				TEXT("clear_focus"),
				TEXT("set_blackboard_value"),
				TEXT("get_blackboard_value"),
				TEXT("run_behavior_tree"),
				TEXT("stop_behavior_tree"),
				TEXT("create"),
				TEXT("add_node"),
				TEXT("connect_nodes"),
				TEXT("remove_node"),
				TEXT("break_connections"),
				TEXT("set_node_properties"),
				TEXT("configure_nav_mesh_settings"),
				TEXT("set_nav_agent_properties"),
				TEXT("rebuild_navigation"),
				TEXT("create_nav_modifier_component"),
				TEXT("set_nav_area_class"),
				TEXT("configure_nav_area_cost"),
				TEXT("configure_nav_link"),
				TEXT("set_nav_link_type"),
				TEXT("create_smart_link"),
				TEXT("configure_smart_link_behavior"),
				TEXT("get_navigation_info")
			}, TEXT("AI action to perform"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
			.String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
			.String(TEXT("blackboardPath"), TEXT("Path to blackboard asset."))
            .String(TEXT("keyName"), TEXT("Name of the key."))
			.StringEnum(TEXT("keyType"), {
				TEXT("Bool"),
				TEXT("Int"),
				TEXT("Float"),
				TEXT("Vector"),
				TEXT("Rotator"),
				TEXT("Object"),
				TEXT("Class"),
				TEXT("Enum"),
				TEXT("Name"),
				TEXT("String")
            }, TEXT("Blackboard key data type."))
            .Bool(TEXT("isInstanceSynced"), TEXT("Sync key across instances."))
            .StringEnum(TEXT("compositeType"), {
				TEXT("Selector"),
				TEXT("Sequence"),
				TEXT("Parallel"),
				TEXT("SimpleParallel")
			}, TEXT("Composite node type."))
			.StringEnum(TEXT("taskType"), {
				TEXT("MoveTo"),
				TEXT("MoveDirectlyToward"),
				TEXT("RotateToFaceBBEntry"),
				TEXT("Wait"),
				TEXT("WaitBlackboardTime"),
				TEXT("PlayAnimation"),
				TEXT("PlaySound"),
				TEXT("RunEQSQuery"),
				TEXT("RunBehaviorDynamic"),
				TEXT("SetBlackboardValue"),
				TEXT("PushPawnAction"),
				TEXT("FinishWithResult"),
				TEXT("MakeNoise"),
				TEXT("GameplayTaskBase"),
				TEXT("Custom")
			}, TEXT("Task node type."))
			.StringEnum(TEXT("decoratorType"), {
				TEXT("Blackboard"),
				TEXT("BlackboardBased"),
				TEXT("CompareBBEntries"),
				TEXT("Cooldown"),
				TEXT("ConeCheck"),
				TEXT("DoesPathExist"),
				TEXT("IsAtLocation"),
				TEXT("IsBBEntryOfClass"),
				TEXT("KeepInCone"),
				TEXT("Loop"),
				TEXT("SetTagCooldown"),
				TEXT("TagCooldown"),
				TEXT("TimeLimit"),
				TEXT("ForceSuccess"),
				TEXT("ConditionalLoop"),
				TEXT("Custom")
			}, TEXT("Decorator node type."))
			.StringEnum(TEXT("serviceType"), {
				TEXT("DefaultFocus"),
				TEXT("RunEQS"),
				TEXT("Custom")
            }, TEXT("Service node type."))
            .String(TEXT("parentNodeId"), TEXT("ID of the node."))
            .String(TEXT("nodeId"), TEXT("ID of the node."))
            .String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
			.StringEnum(TEXT("generatorType"), {
				TEXT("ActorsOfClass"),
				TEXT("CurrentLocation"),
				TEXT("Donut"),
				TEXT("OnCircle"),
				TEXT("PathingGrid"),
				TEXT("SimpleGrid"),
				TEXT("Composite"),
				TEXT("Custom")
			}, TEXT("EQS generator type."))
			.StringEnum(TEXT("contextType"), {
				TEXT("Querier"),
				TEXT("Item"),
				TEXT("EnvQueryContext_BlueprintBase"),
				TEXT("Custom")
			}, TEXT("EQS context type."))
			.StringEnum(TEXT("testType"), {
				TEXT("Distance"),
				TEXT("Dot"),
				TEXT("GameplayTags"),
				TEXT("Overlap"),
				TEXT("Pathfinding"),
				TEXT("PathfindingBatch"),
				TEXT("Project"),
				TEXT("Random"),
				TEXT("Trace"),
				TEXT("Custom")
			}, TEXT("EQS test type."))
			.Object(TEXT("generatorSettings"), TEXT("Generator-specific settings."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("searchRadius"))
				 .String(TEXT("searchCenter"), TEXT(""))
				 .String(TEXT("actorClass"), TEXT(""))
				 .Number(TEXT("gridSize"))
				 .Number(TEXT("spacesBetween"))
				 .Number(TEXT("innerRadius"))
				 .Number(TEXT("outerRadius"));
			})
			.Object(TEXT("testSettings"), TEXT("Test scoring and filter settings."),
				[](FMcpSchemaBuilder& S) {
				S.StringEnum(TEXT("scoringEquation"), {
					TEXT("Linear"),
					TEXT("Square"),
					TEXT("InverseLinear"),
					TEXT("Constant")
				}, TEXT(""))
				 .Number(TEXT("clampMin"))
				 .Number(TEXT("clampMax"))
				 .StringEnum(TEXT("filterType"), {
					TEXT("Minimum"),
					TEXT("Maximum"),
					TEXT("Range")
				}, TEXT(""))
				 .Number(TEXT("floatMin"))
				 .Number(TEXT("floatMax"));
			})
			.Number(TEXT("testIndex"), TEXT("Index of test to configure."))
			.Object(TEXT("sightConfig"), TEXT("AI sight sense configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("sightRadius"))
				 .Number(TEXT("loseSightRadius"))
				 .Number(TEXT("peripheralVisionAngle"))
				 .Number(TEXT("pointOfViewBackwardOffset"))
				 .Number(TEXT("nearClippingRadius"))
				 .Number(TEXT("autoSuccessRange"))
				 .Number(TEXT("maxAge"))
				 .Object(TEXT("detectionByAffiliation"), TEXT(""),
					[](FMcpSchemaBuilder& Inner) {
					Inner.Bool(TEXT("enemies"), TEXT(""))
						 .Bool(TEXT("neutrals"), TEXT(""))
						 .Bool(TEXT("friendlies"), TEXT(""));
				});
			})
			.Object(TEXT("hearingConfig"), TEXT("AI hearing sense configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("hearingRange"))
				 .Number(TEXT("loSHearingRange"))
				 .Bool(TEXT("detectFriendly"), TEXT(""))
				 .Number(TEXT("maxAge"));
			})
			.Object(TEXT("damageConfig"), TEXT("AI damage sense configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("maxAge"));
			})
			.Number(TEXT("teamId"), TEXT("Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.)."))
			.StringEnum(TEXT("dominantSense"), {
				TEXT("Sight"),
				TEXT("Hearing"),
				TEXT("Damage"),
				TEXT("Touch"),
				TEXT("None")
			}, TEXT("Dominant sense for perception prioritization."))
			.String(TEXT("stateTreePath"), TEXT("Path to State Tree asset."))
            .String(TEXT("stateName"), TEXT("Name of the state."))
            .String(TEXT("fromState"), TEXT("Source state name."))
            .String(TEXT("toState"), TEXT("Target state name."))
            .String(TEXT("definitionPath"), TEXT("Path to definition asset."))
            .Number(TEXT("slotIndex"), TEXT("Index of slot to configure."))
            .String(TEXT("configPath"), TEXT("Path to config asset."))
            .String(TEXT("actorName"), TEXT("Name of the actor."))
            .Number(TEXT("agentRadius"), TEXT("Navigation agent radius (default: 35)."))
			.Number(TEXT("agentHeight"), TEXT("Navigation agent height (default: 144)."))
			.Number(TEXT("agentStepHeight"), TEXT("Maximum step height agent can climb (default: 35)."))
			.Number(TEXT("agentMaxSlope"), TEXT("Maximum slope angle in degrees (default: 44)."))
			.Number(TEXT("cellSize"), TEXT("NavMesh cell size (default: 19)."))
			.Number(TEXT("cellHeight"), TEXT("NavMesh cell height (default: 10)."))
			.Number(TEXT("tileSizeUU"), TEXT("NavMesh tile size in UU (default: 1000)."))
			.Number(TEXT("minRegionArea"), TEXT("Minimum region area to keep."))
			.Number(TEXT("mergeRegionSize"), TEXT("Region merge threshold."))
			.Number(TEXT("maxSimplificationError"), TEXT("Edge simplification error."))
            .String(TEXT("componentName"), TEXT("Name of the component."))
            .String(TEXT("areaClass"), TEXT("Navigation area class path."))
            .Object(TEXT("failsafeExtent"), TEXT("Failsafe extent for nav modifier when actor has no collision."),
                [](FMcpSchemaBuilder& S) {
                S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
            })
            .Number(TEXT("areaCost"), TEXT("Pathfinding cost multiplier for area (1.0 = normal)."))
            .Object(TEXT("startPoint"), TEXT("Start point of navigation link (relative to actor)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("endPoint"), TEXT("End point of navigation link (relative to actor)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.StringEnum(TEXT("direction"), {
				TEXT("BothWays"),
				TEXT("LeftToRight"),
				TEXT("RightToLeft")
			}, TEXT("Link traversal direction."))
			.Number(TEXT("snapRadius"), TEXT("Snap radius for link endpoints (default: 30)."))
			.Bool(TEXT("linkEnabled"), TEXT("Whether the link is enabled."))
            .StringEnum(TEXT("linkType"), {
                TEXT("simple"),
                TEXT("smart")
            }, TEXT("Type of navigation link."))
            .String(TEXT("enabledAreaClass"), TEXT("Area class when smart link is enabled."))
			.String(TEXT("disabledAreaClass"), TEXT("Area class when smart link is disabled."))
			.Number(TEXT("broadcastRadius"), TEXT("Radius for state change broadcast."))
			.Number(TEXT("broadcastInterval"), TEXT("Interval for state change broadcast (0 = single)."))
			.Bool(TEXT("bCreateBoxObstacle"), TEXT("Add box obstacle during nav generation."))
			.Object(TEXT("obstacleOffset"), TEXT("Offset of simple box obstacle."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("obstacleExtent"), TEXT("Extent of simple box obstacle."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
            })
            .String(TEXT("obstacleAreaClass"), TEXT("Area class for box obstacle."))
            .Object(TEXT("location"), TEXT("World location for nav link proxy."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("Rotation for nav link proxy."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
            })
            .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("nodeType"), TEXT("Behavior Tree graph node type."))
			.String(TEXT("childNodeId"), TEXT("Child node ID."))
			.Number(TEXT("x"), TEXT("Graph node X coordinate."))
			.Number(TEXT("y"), TEXT("Graph node Y coordinate."))
			.String(TEXT("comment"), TEXT("Node comment."))
			.FreeformObject(TEXT("properties"), TEXT("Properties to set on a graph node."))
			.Bool(TEXT("enableDamage"), TEXT("Enable damage sense."))
			.Bool(TEXT("enableHearing"), TEXT("Enable hearing sense."))
			.Bool(TEXT("enableSight"), TEXT("Enable sight sense."))
			.Bool(TEXT("enabled"), TEXT("Generic enabled flag."))
			.String(TEXT("focusActorName"), TEXT("Actor name to focus."))
			.Number(TEXT("hearingRange"), TEXT("AI hearing range."))
			.Number(TEXT("loseSightRadius"), TEXT("AI sight lose radius."))
			.Object(TEXT("offset"), TEXT("Generic offset vector."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("parentStateName"), TEXT("Parent state name."))
			.Number(TEXT("peripheralVisionAngle"), TEXT("Sight peripheral vision angle."))
			.Number(TEXT("sightRadius"), TEXT("AI sight radius."))
			.Number(TEXT("spawnCount"), TEXT("Mass spawner entity count."))
			.String(TEXT("stateType"), TEXT("State Tree state type."))
			.String(TEXT("triggerType"), TEXT("State Tree transition trigger type."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAI);

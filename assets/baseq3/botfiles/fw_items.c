// How much bots want each item (the goal weights), on one scale of our own
// (bots-audit BOT-02, BOT-07): the mega health strongly under 100 health,
// the armours under their caps, weapons when they don't have them (their ammo
// when they do), the Quad and Protection above all, the hourglass while the
// ability recharges. Item names are the classnames of items.c. Our own data;
// the stock bots' item personalities (their W_ defines) are not used.

weight "item_armor_shard"
{
	switch(INVENTORY_ABILITYWAIT)
	{
		case 1: return 10;
		default: return 60;
	}
}

weight "item_armor_combat"
{
	switch(INVENTORY_ARMOR)
	{
		case 100: return 160;
		case 150: return 90;
		default: return 0;
	}
}

weight "item_armor_body"
{
	switch(INVENTORY_ARMOR)
	{
		case 100: return 260;
		case 200: return 160;
		default: return 0;
	}
}

weight "item_health_small"
{
	switch(INVENTORY_HEALTH)
	{
		case 50: return 60;
		case 100: return 30;
		default: return 5;
	}
}

weight "item_health"
{
	switch(INVENTORY_HEALTH)
	{
		case 50: return 120;
		case 100: return 60;
		default: return 0;
	}
}

weight "item_health_large"
{
	switch(INVENTORY_HEALTH)
	{
		case 50: return 160;
		case 100: return 90;
		default: return 0;
	}
}

weight "item_health_special"
{
	switch(INVENTORY_HEALTH)
	{
		case 50: return 160;
		case 100: return 90;
		default: return 0;
	}
}

weight "item_health_mega"
{
	switch(INVENTORY_HEALTH)
	{
		case 100: return 300;
		case 175: return 220;
		default: return 0;
	}
}

weight "weapon_gauntlet"
{
	return 0;
}

weight "weapon_shotgun"
{
	switch(INVENTORY_SSG)
	{
		case 1: return 140;
		default:
		{
			switch(INVENTORY_SHELLS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_supershotgun"
{
	switch(INVENTORY_SSG)
	{
		case 1: return 140;
		default:
		{
			switch(INVENTORY_SHELLS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_ssg"
{
	switch(INVENTORY_SSG)
	{
		case 1: return 140;
		default:
		{
			switch(INVENTORY_SHELLS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_machinegun"
{
	switch(INVENTORY_HMG)
	{
		case 1: return 140;
		default:
		{
			switch(INVENTORY_BULLETS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_heavymachinegun"
{
	switch(INVENTORY_HMG)
	{
		case 1: return 140;
		default:
		{
			switch(INVENTORY_BULLETS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_hmg"
{
	switch(INVENTORY_HMG)
	{
		case 1: return 140;
		default:
		{
			switch(INVENTORY_BULLETS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_grenadelauncher"
{
	switch(INVENTORY_GRENADELAUNCHER)
	{
		case 1: return 120;
		default:
		{
			switch(INVENTORY_GRENADES)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_tribolt"
{
	switch(INVENTORY_GRENADELAUNCHER)
	{
		case 1: return 120;
		default:
		{
			switch(INVENTORY_GRENADES)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_rocketlauncher"
{
	switch(INVENTORY_ROCKETLAUNCHER)
	{
		case 1: return 180;
		default:
		{
			switch(INVENTORY_ROCKETS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_lightning"
{
	switch(INVENTORY_LIGHTNING)
	{
		case 1: return 180;
		default:
		{
			switch(INVENTORY_LIGHTNINGAMMO)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_railgun"
{
	switch(INVENTORY_RAILGUN)
	{
		case 1: return 200;
		default:
		{
			switch(INVENTORY_SLUGS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_bfg"
{
	switch(INVENTORY_RAILGUN)
	{
		case 1: return 200;
		default:
		{
			switch(INVENTORY_SLUGS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_plasmagun"
{
	switch(INVENTORY_PLASMAGUN)
	{
		case 1: return 160;
		default:
		{
			switch(INVENTORY_CELLS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_supernailgun"
{
	switch(INVENTORY_PLASMAGUN)
	{
		case 1: return 160;
		default:
		{
			switch(INVENTORY_CELLS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_nailgun"
{
	switch(INVENTORY_NAILGUN)
	{
		case 1: return 90;
		default:
		{
			switch(INVENTORY_CELLS)
			{
				case 10: return 20;
				default: return 0;
			}
		}
	}
}

weight "weapon_grapplinghook"
{
	return 0;
}

weight "ammo_shells"
{
	switch(INVENTORY_SHELLS)
	{
		case 10: return 50;
		case 20: return 20;
		default: return 0;
	}
}

weight "ammo_bullets"
{
	switch(INVENTORY_BULLETS)
	{
		case 50: return 50;
		case 100: return 20;
		default: return 0;
	}
}

weight "ammo_grenades"
{
	switch(INVENTORY_GRENADES)
	{
		case 5: return 50;
		case 10: return 20;
		default: return 0;
	}
}

weight "ammo_bolts"
{
	switch(INVENTORY_GRENADES)
	{
		case 5: return 50;
		case 10: return 20;
		default: return 0;
	}
}

weight "ammo_cells"
{
	switch(INVENTORY_CELLS)
	{
		case 50: return 50;
		case 100: return 20;
		default: return 0;
	}
}

weight "ammo_lightning"
{
	switch(INVENTORY_LIGHTNINGAMMO)
	{
		case 50: return 50;
		case 100: return 20;
		default: return 0;
	}
}

weight "ammo_rockets"
{
	switch(INVENTORY_ROCKETS)
	{
		case 5: return 60;
		case 10: return 25;
		default: return 0;
	}
}

weight "ammo_slugs"
{
	switch(INVENTORY_SLUGS)
	{
		case 5: return 60;
		case 10: return 25;
		default: return 0;
	}
}

weight "ammo_bfg"
{
	switch(INVENTORY_SLUGS)
	{
		case 5: return 60;
		case 10: return 25;
		default: return 0;
	}
}

weight "holdable_teleporter"
{
	switch(INVENTORY_TELEPORTER)
	{
		case 1: return 80;
		default: return 0;
	}
}

weight "holdable_medkit"
{
	switch(INVENTORY_MEDKIT)
	{
		case 1: return 80;
		default: return 0;
	}
}

weight "item_quad"
{
	return 450;
}

weight "item_enviro"
{
	return 350;
}

weight "item_protection"
{
	return 350;
}

weight "item_haste"
{
	return 120;
}

weight "item_invis"
{
	return 150;
}

weight "item_regen"
{
	return 200;
}

weight "item_flight"
{
	return 0;
}

weight "team_CTF_redflag"
{
	return 0;
}

weight "team_CTF_blueflag"
{
	return 0;
}

weight "item_hourglass"
{
	switch(INVENTORY_ABILITYWAIT)
	{
		case 1: return 10;
		default: return 60;
	}
}

weight "item_ammo_box"
{
	return 40;
}
